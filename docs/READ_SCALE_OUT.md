# 读性能 Scale Out 和一致性视图设计文档

## 1. 概述

本文档描述了 KV 存储引擎的读性能 Scale Out 策略和一致性视图设计。该设计通过多版本并发控制（MVCC）和快照隔离机制，实现高并发读取和一致性视图保证。

### 1.1 设计目标

- **高并发读取**: 支持 1000+ 并发读取线程，读取吞吐量 100K+ QPS
- **无锁读取**: 读取路径完全无锁，读取操作不阻塞写入，写入操作不阻塞读取
- **一致性视图**: 提供快照隔离级别的一致性保证，支持事务性读取
- **线性扩展**: 读取性能随线程数线性增长（直到 I/O 瓶颈）

### 1.2 核心机制

- **多版本并发控制 (MVCC)**: 通过版本快照实现无锁读取
- **版本引用计数**: 通过引用计数管理版本生命周期
- **快照隔离**: 通过序列号实现快照一致性视图
- **并行查找**: 在多个 SSTable 文件中并行查找，提高性能

## 2. 读性能 Scale Out 策略

### 2.1 多版本并发控制 (MVCC)

**设计目标**: 支持高并发读取，读取操作不阻塞写入，写入操作不阻塞读取。

**核心机制**:
- **版本快照**: 每个读取操作获取一个版本快照，读取过程中版本不变
- **无锁读取**: 读取路径完全无锁，通过引用计数管理版本生命周期
- **版本链**: 通过版本链维护多个历史版本，支持快照读取

**实现方式**:
```cpp
class DB {
public:
    // 获取快照（用于一致性读取）
    const Snapshot* GetSnapshot();
    
    // 释放快照
    void ReleaseSnapshot(const Snapshot* snapshot);
    
    // 带快照的读取
    Status Get(const ReadOptions& options,
               const std::string& key,
               std::string* value);
    
private:
    VersionSet* versions_;
    std::atomic<uint64_t> sequence_number_;
};

class Snapshot {
public:
    uint64_t sequence_number() const { return sequence_; }
    
private:
    uint64_t sequence_;  // 快照对应的序列号
    Version* version_;    // 快照对应的版本
    friend class DB;
};
```

### 2.2 版本引用计数管理

**设计原则**: 
- 读取操作获取版本引用，确保读取期间版本不被删除
- 版本只有在所有引用释放后才被删除
- 使用原子操作管理引用计数，保证线程安全

**实现细节**:
```cpp
class Version {
public:
    // 增加引用计数（读取时调用）
    void Ref() {
        refs_.fetch_add(1, std::memory_order_relaxed);
    }
    
    // 减少引用计数（读取完成时调用）
    void Unref() {
        int refs = refs_.fetch_sub(1, std::memory_order_acq_rel);
        if (refs == 1) {
            // 最后一个引用，可以安全删除
            delete this;
        }
    }
    
private:
    std::atomic<int> refs_;  // 引用计数
    uint64_t sequence_number_;  // 版本序列号
};
```

### 2.3 读取路径优化

**优化策略**:

1. **版本缓存**: 缓存常用的版本，减少版本查找开销
   ```cpp
   class VersionCache {
   public:
       Version* GetVersion(uint64_t sequence);
       void PutVersion(uint64_t sequence, Version* version);
       
   private:
       std::map<uint64_t, Version*> cache_;
       size_t max_size_;
   };
   ```

2. **并行查找**: 在多个 SSTable 文件中并行查找
   ```cpp
   // Level 0 文件可以并行查找（可能有重叠）
   std::vector<std::future<Status>> futures;
   for (auto& file : level0_files) {
       futures.push_back(std::async(std::launch::async, 
           [&]() { return file->Get(key, value); }));
   }
   ```

3. **早期终止**: 找到数据后立即返回，不继续查找
   ```cpp
   // 按文件编号倒序查找（新的在前）
   for (auto it = files.rbegin(); it != files.rend(); ++it) {
       Status s = (*it)->Get(key, value);
       if (s.ok() || !s.IsNotFound()) {
           return s;  // 找到或出错，立即返回
       }
   }
   ```

4. **Bloom Filter 预过滤**: 使用 Bloom Filter 快速判断 key 是否存在
   ```cpp
   // 在读取 SSTable 前先检查 Bloom Filter
   if (!bloom_filter->MayContain(key)) {
       return Status::NotFound();  // 快速返回
   }
   ```

### 2.4 读取并发度提升

**设计要点**:

- **无锁数据结构**: MemTable 使用无锁数据结构（如无锁跳表）
- **文件句柄池**: 复用文件句柄，减少文件打开/关闭开销
- **Block Cache 共享**: 多个读取线程共享 Block Cache
- **预取优化**: 根据访问模式预取相邻 Block

**性能指标**:
- **读取吞吐量**: 目标支持 100K+ QPS（单机）
- **读取延迟**: P99 延迟 < 1ms（缓存命中）
- **并发度**: 支持 1000+ 并发读取线程

## 3. 一致性视图设计

### 3.1 快照隔离 (Snapshot Isolation)

**一致性级别**: 提供快照隔离级别的一致性保证。

**核心特性**:
- **读一致性**: 同一快照内的所有读取看到相同的数据版本
- **写一致性**: 写入操作是原子的，要么全部成功，要么全部失败
- **隔离性**: 不同快照之间互不干扰

**实现机制**:
```cpp
class ReadOptions {
public:
    // 指定快照（如果不指定，使用最新版本）
    const Snapshot* snapshot;
    
    // 读取一致性级别
    enum ConsistencyLevel {
        kStrongConsistency,    // 强一致性（读取最新提交的数据）
        kSnapshotConsistency,  // 快照一致性（读取指定快照的数据）
        kEventualConsistency  // 最终一致性（允许读取稍旧的数据）
    };
    ConsistencyLevel consistency;
};
```

### 3.2 序列号管理

**序列号作用**:
- **版本标识**: 每个写入操作分配唯一的序列号
- **排序保证**: 序列号单调递增，保证操作顺序
- **快照标记**: 快照通过序列号标识数据版本

**序列号分配**:
```cpp
class DB {
private:
    // 全局序列号生成器
    std::atomic<uint64_t> sequence_number_;
    
    // 为写入操作分配序列号
    uint64_t AllocateSequenceNumber() {
        return sequence_number_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
    
    // 获取当前最大序列号
    uint64_t GetLastSequenceNumber() const {
        return sequence_number_.load(std::memory_order_acquire);
    }
};
```

**序列号在数据中的使用**:
```cpp
// WAL 记录包含序列号
struct WALRecord {
    RecordType type;
    uint64_t sequence;  // 序列号
    std::string key;
    std::string value;
    uint32_t checksum;
};

// SSTable Entry 包含序列号（用于判断可见性）
struct SSTableEntry {
    std::string key;
    std::string value;
    uint64_t sequence;  // 写入时的序列号
    bool is_deletion;   // 是否为删除标记
};
```

### 3.3 版本可见性判断

**可见性规则**:
- 读取操作只能看到序列号 <= 快照序列号的数据
- 删除操作通过序列号标记，旧版本数据对快照不可见
- 同一 key 的多个版本中，只有最新的可见版本被返回

**实现逻辑**:
```cpp
Status Version::Get(const ReadOptions& options,
                    const std::string& key,
                    std::string* value) {
    uint64_t snapshot_seq = options.snapshot ? 
        options.snapshot->sequence_number() : 
        GetLastSequenceNumber();
    
    // 1. 查找 MemTable（包含序列号）
    Status s = memtable_->Get(key, snapshot_seq, value);
    if (s.ok() || !s.IsNotFound()) {
        return s;
    }
    
    // 2. 查找 Immutable MemTable
    s = immutable_memtable_->Get(key, snapshot_seq, value);
    if (s.ok() || !s.IsNotFound()) {
        return s;
    }
    
    // 3. 查找 SSTable（按文件编号倒序）
    for (int level = 0; level < kNumLevels; ++level) {
        for (auto it = files_[level].rbegin(); 
             it != files_[level].rend(); ++it) {
            s = (*it)->Get(key, snapshot_seq, value);
            if (s.ok()) {
                return s;
            } else if (!s.IsNotFound()) {
                return s;  // 错误
            }
            // NotFound 继续查找
        }
    }
    
    return Status::NotFound();
}
```

### 3.4 快照生命周期管理

**快照创建**:
```cpp
const Snapshot* DB::GetSnapshot() {
    // 获取当前版本和序列号
    Version* version = versions_->current();
    uint64_t sequence = versions_->GetLastSequenceNumber();
    
    // 创建快照
    Snapshot* snapshot = new Snapshot(sequence, version);
    version->Ref();  // 增加版本引用计数
    
    // 注册快照
    snapshots_.insert(snapshot);
    
    return snapshot;
}
```

**快照释放**:
```cpp
void DB::ReleaseSnapshot(const Snapshot* snapshot) {
    // 从快照列表移除
    snapshots_.erase(snapshot);
    
    // 释放版本引用
    snapshot->version()->Unref();
    
    // 删除快照对象
    delete snapshot;
    
    // 清理不再需要的版本
    versions_->CleanupOldVersions();
}
```

**版本清理策略**:
```cpp
void VersionSet::CleanupOldVersions() {
    // 找到所有活跃快照的最小序列号
    uint64_t min_snapshot_seq = GetMinSnapshotSequence();
    
    // 删除序列号 < min_snapshot_seq 的版本
    for (auto it = version_cache_.begin(); 
         it != version_cache_.end();) {
        if (it->first < min_snapshot_seq && 
            it->second->refs() == 0) {
            delete it->second;
            it = version_cache_.erase(it);
        } else {
            ++it;
        }
    }
}
```

### 3.5 一致性保证级别

**强一致性 (Strong Consistency)**:
- 读取最新提交的数据
- 使用当前最大序列号作为快照序列号
- 适用于需要最新数据的场景

**快照一致性 (Snapshot Consistency)**:
- 读取指定快照的数据
- 通过 GetSnapshot() 获取快照
- 适用于需要一致性视图的场景（如事务、报表）

**最终一致性 (Eventual Consistency)**:
- 允许读取稍旧的数据
- 使用较小的序列号作为快照序列号
- 适用于可以容忍短暂延迟的场景

### 3.6 并发控制机制

**读取路径（无锁）**:
```
1. 获取版本引用（原子操作）
2. 读取数据（无锁）
3. 释放版本引用（原子操作）
```

**写入路径（最小锁）**:
```
1. 获取写入锁（保护 MemTable）
2. 分配序列号
3. 写入 WAL
4. 写入 MemTable
5. 释放写入锁
```

**Compaction 路径（版本锁）**:
```
1. 获取版本锁
2. 创建新版本
3. 更新 VersionSet
4. 释放版本锁
5. 后台删除旧版本（等待引用计数为 0）
```

## 4. 读性能 Scale Out 架构

### 4.1 单机多线程读取

**架构设计**:
```
┌─────────────────────────────────────────────────────────┐
│                    Read Thread Pool                      │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐        │
│  │Thread 1│  │Thread 2│  │Thread 3│  │Thread N│        │
│  └────────┘  └────────┘  └────────┘  └────────┘        │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                  Shared Version Set                       │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Current Version (原子引用计数)                    │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Version Cache (快照版本缓存)                     │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│              Shared Block Cache                          │
│  (多线程共享，线程安全)                                    │
└─────────────────────────────────────────────────────────┘
```

**性能特点**:
- **线性扩展**: 读取线程数增加，吞吐量线性增长（直到 I/O 瓶颈）
- **无锁读取**: 读取路径完全无锁，无竞争
- **缓存共享**: Block Cache 多线程共享，提高命中率

### 4.2 读取路径性能优化

**优化点**:

1. **版本查找优化**:
   ```cpp
   // 使用哈希表加速版本查找
   class VersionCache {
   private:
       std::unordered_map<uint64_t, Version*> cache_;
       std::shared_mutex mutex_;  // 读写锁
   };
   ```

2. **并行 SSTable 查找**:
   ```cpp
   // Level 0 文件并行查找
   std::vector<std::future<Status>> results;
   for (const auto& file : level0_files) {
       results.push_back(std::async(std::launch::async,
           [&]() { return file->Get(key, snapshot_seq, value); }));
   }
   
   // 等待第一个成功的结果
   for (auto& result : results) {
       Status s = result.get();
       if (s.ok()) return s;
   }
   ```

3. **预取优化**:
   ```cpp
   // 读取 Block 时预取相邻 Block
   void PrefetchAdjacentBlocks(const BlockHandle& current) {
       // 预取下一个 Block
       block_cache_->Prefetch(GetNextBlockHandle(current));
   }
   ```

### 4.3 一致性视图的性能影响

**性能权衡**:

| 一致性级别 | 读取延迟 | 吞吐量 | 一致性保证 |
|-----------|---------|--------|-----------|
| 强一致性 | 中等 | 高 | 最强 |
| 快照一致性 | 低 | 最高 | 中等 |
| 最终一致性 | 最低 | 最高 | 最弱 |

**优化建议**:
- **默认使用快照一致性**: 平衡性能和一致性
- **关键操作使用强一致性**: 需要最新数据时
- **批量读取使用最终一致性**: 可以容忍延迟时

## 5. 一致性视图的使用场景

### 5.1 事务性读取

```cpp
// 事务开始：获取快照
const Snapshot* snapshot = db->GetSnapshot();

// 事务内的所有读取使用同一快照
ReadOptions options;
options.snapshot = snapshot;

std::string value1, value2;
db->Get(options, "key1", &value1);
db->Get(options, "key2", &value2);

// 事务结束：释放快照
db->ReleaseSnapshot(snapshot);
```

### 5.2 报表和统计

```cpp
// 报表生成：使用快照保证数据一致性
const Snapshot* report_snapshot = db->GetSnapshot();

// 遍历所有数据生成报表
Iterator* it = db->NewIterator(ReadOptions().snapshot(report_snapshot));
for (it->SeekToFirst(); it->Valid(); it->Next()) {
    // 处理数据...
}

db->ReleaseSnapshot(report_snapshot);
```

### 5.3 时间点恢复

```cpp
// 恢复到指定时间点的数据
uint64_t target_sequence = GetSequenceForTimestamp(timestamp);
const Snapshot* recovery_snapshot = CreateSnapshot(target_sequence);

// 使用恢复快照读取数据
ReadOptions options;
options.snapshot = recovery_snapshot;
// ... 读取操作
```

## 6. 接口设计

### 6.1 DB 接口扩展

```cpp
class DB {
public:
    // 获取快照（用于一致性读取）
    const Snapshot* GetSnapshot();
    
    // 释放快照
    void ReleaseSnapshot(const Snapshot* snapshot);
    
    // 带快照的读取
    Status Get(const ReadOptions& options,
               const std::string& key,
               std::string* value);
    
    // 带快照的迭代器
    Iterator* NewIterator(const ReadOptions& options);
    
private:
    VersionSet* versions_;
    std::atomic<uint64_t> sequence_number_;
    std::set<const Snapshot*> snapshots_;
};
```

### 6.2 ReadOptions 扩展

```cpp
struct ReadOptions {
    // 现有选项...
    
    // 一致性视图相关选项
    const Snapshot* snapshot;       // 快照（用于一致性读取）
    enum ConsistencyLevel {
        kStrongConsistency,        // 强一致性（默认）
        kSnapshotConsistency,      // 快照一致性
        kEventualConsistency       // 最终一致性
    };
    ConsistencyLevel consistency;  // 一致性级别（默认 kStrongConsistency）
    
    // 读取优化选项
    bool fill_cache;               // 是否填充缓存（默认 true）
    bool verify_checksums;         // 是否验证校验和（默认 false）
};
```

### 6.3 Options 扩展

```cpp
struct Options {
    // 现有选项...
    
    // 读性能 Scale Out 相关选项
    size_t block_cache_size;       // Block Cache 大小（默认 8MB）
    size_t version_cache_size;     // 版本缓存大小（默认 100）
    int max_read_threads;          // 最大读取线程数（默认 CPU 核心数）
};
```

## 7. 实现计划

### 7.1 第一阶段：序列号和版本管理

- [ ] 实现序列号生成器（原子操作）
- [ ] 实现版本引用计数管理
- [ ] 实现版本创建和删除机制
- [ ] 测试版本生命周期管理

### 7.2 第二阶段：快照机制

- [ ] 实现 Snapshot 类
- [ ] 实现 GetSnapshot() 和 ReleaseSnapshot()
- [ ] 实现快照注册和清理
- [ ] 测试快照创建和释放

### 7.3 第三阶段：版本可见性

- [ ] 在 WAL 记录中添加序列号
- [ ] 在 SSTable Entry 中添加序列号
- [ ] 实现版本可见性判断逻辑
- [ ] 测试不同序列号的可见性

### 7.4 第四阶段：无锁读取路径

- [ ] 实现无锁版本引用获取
- [ ] 实现无锁 MemTable 读取（无锁跳表）
- [ ] 实现无锁 SSTable 读取
- [ ] 测试并发读取正确性

### 7.5 第五阶段：性能优化

- [ ] 实现版本缓存
- [ ] 实现并行 SSTable 查找
- [ ] 实现预取优化
- [ ] 性能基准测试

### 7.6 第六阶段：测试和验证

- [ ] 并发读取压力测试
- [ ] 一致性视图正确性测试
- [ ] 性能回归测试
- [ ] 文档完善

## 8. 性能目标

### 8.1 吞吐量目标

- **单线程读取**: 10K+ QPS
- **多线程读取**: 100K+ QPS（10+ 线程）
- **并发读取**: 支持 1000+ 并发读取线程

### 8.2 延迟目标

- **缓存命中**: P50 < 0.1ms, P99 < 1ms
- **缓存未命中**: P50 < 1ms, P99 < 10ms
- **磁盘读取**: P50 < 5ms, P99 < 50ms

### 8.3 一致性目标

- **快照一致性**: 100% 正确性
- **强一致性**: 读取最新数据，延迟 < 10ms
- **最终一致性**: 允许最多 100ms 延迟

## 9. 参考设计

本设计参考了以下存储系统的实现：

- **LevelDB**: Google 的键值存储库，MVCC 和快照机制的核心参考
- **RocksDB**: Facebook 基于 LevelDB 的优化版本，多线程读取优化
- **PostgreSQL**: 数据库系统，快照隔离的实现参考
- **CockroachDB**: 分布式数据库，一致性视图的实现参考

## 10. 总结

本设计文档描述了读性能 Scale Out 和一致性视图的完整实现方案。该方案通过 MVCC 和快照隔离机制，实现了：

- **高并发读取**: 支持 1000+ 并发读取线程，吞吐量 100K+ QPS
- **无锁读取**: 读取路径完全无锁，无竞争
- **一致性视图**: 提供快照隔离级别的一致性保证
- **线性扩展**: 读取性能随线程数线性增长

实现将分阶段进行，确保每个阶段都有完整的测试覆盖和性能验证。
