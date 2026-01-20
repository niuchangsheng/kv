# 持久化存储设计文档 (WAL + SSTable)

## 1. 概述

本文档描述了基于 WAL (Write-Ahead Logging) 和 SSTable (Sorted String Table) 的持久化存储实现设计。该设计将当前的内存存储引擎扩展为支持数据持久化的存储系统。

### 1.1 设计目标

- **数据持久化**: 数据写入磁盘，进程重启后数据不丢失
- **高性能写入**: 使用 WAL 实现顺序写入，提高写入性能
- **高效读取**: 使用 SSTable 实现有序存储，支持快速查找
- **数据可靠性**: 通过 WAL 和校验和确保数据完整性
- **可恢复性**: 支持数据库崩溃后的数据恢复
- **可扩展性**: 支持数据增长和 Compaction 操作

### 1.2 核心组件

- **WAL (Write-Ahead Log)**: 预写日志，记录所有写操作
- **MemTable**: 内存中的有序表，用于快速写入和读取
- **SSTable**: 磁盘上的有序字符串表，不可变的数据文件
- **Manifest**: 元数据文件，记录所有 SSTable 文件的信息
- **Compaction**: 数据合并和压缩机制

## 2. 系统架构

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│              (DB::Put, DB::Get, DB::Delete)                 │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                         DB Layer                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   MemTable   │  │      WAL     │  │   VersionSet │      │
│  │ (In-Memory)  │  │  (Log File)  │  │  (Manifest)  │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      Storage Layer                           │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              SSTable Files (Level 0)                   │  │
│  │  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐   │  │
│  │  │SST-001 │  │SST-002 │  │SST-003 │  │SST-004 │   │  │
│  │  └────────┘  └────────┘  └────────┘  └────────┘   │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              SSTable Files (Level 1)                   │  │
│  │  ┌────────┐  ┌────────┐                              │  │
│  │  │SST-101 │  │SST-102 │                              │  │
│  │  └────────┘  └────────┘                              │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 数据流程

#### 写入流程 (Put/Delete)

```
1. 写入 WAL
   └─► 追加日志记录到 WAL 文件
   
2. 写入 MemTable
   └─► 插入/更新内存中的有序表
   
3. MemTable 满时
   └─► 转换为 Immutable MemTable
   └─► 后台线程写入 SSTable (Level 0)
   └─► 创建新的 MemTable
```

#### 读取流程 (Get)

```
1. 检查 MemTable
   └─► 如果找到，返回结果
   
2. 检查 Immutable MemTable
   └─► 如果找到，返回结果
   
3. 从 Level 0 开始查找 SSTable
   └─► 按文件编号倒序查找（新的在前）
   └─► 如果找到，返回结果
   
4. 继续查找 Level 1, 2, ...
   └─► 使用 Bloom Filter 快速过滤
   └─► 二分查找定位数据
```

## 3. WAL (Write-Ahead Log) 设计

### 3.1 WAL 的作用

- **持久化保证**: 所有写操作先写入 WAL，确保数据不丢失
- **崩溃恢复**: 数据库重启时从 WAL 恢复数据
- **顺序写入**: 追加写入，性能高

### 3.2 WAL 文件格式

#### 文件结构

```
WAL File: <db_name>/LOG
Size: 可变
Format: 一系列 Record

Record Format:
┌─────────────┬──────────────┬─────────────┬─────────────┐
│ Record Type │ Key Length   │ Value Length│     Key     │
│   (1 byte)  │  (4 bytes)   │  (4 bytes)  │ (variable)  │
├─────────────┼──────────────┼─────────────┼─────────────┤
│    Value    │   Checksum   │             │             │
│ (variable)  │  (4 bytes)   │             │             │
└─────────────┴──────────────┴─────────────┴─────────────┘
```

#### Record Type

```cpp
enum RecordType {
    kPut = 1,      // Put 操作
    kDelete = 2,   // Delete 操作
    kSync = 3,     // 同步点（强制刷盘）
    kEof = 4       // 文件结束标记
};
```

#### 示例记录

```
Put 操作:
Type: 0x01
Key Length: 0x00000003 ("key")
Value Length: 0x00000005 ("value")
Key: "key"
Value: "value"
Checksum: CRC32(key + value)

Delete 操作:
Type: 0x02
Key Length: 0x00000003 ("key")
Value Length: 0x00000000
Key: "key"
Value: (empty)
Checksum: CRC32(key)
```

### 3.3 WAL 实现细节

#### WAL Writer

```cpp
class WALWriter {
public:
    WALWriter(const std::string& log_file);
    ~WALWriter();
    
    Status AddRecord(RecordType type, 
                     const std::string& key, 
                     const std::string& value);
    Status Sync();  // 强制刷盘
    Status Close();
    
private:
    std::ofstream file_;
    uint32_t block_offset_;  // 当前块内偏移
    static const size_t kBlockSize = 32768;  // 32KB 块大小
};
```

#### WAL Reader

```cpp
class WALReader {
public:
    WALReader(const std::string& log_file);
    ~WALReader();
    
    // 读取并应用所有记录到 MemTable
    Status Replay(MemTable* memtable);
    
    // 读取单条记录
    bool ReadRecord(RecordType* type,
                    std::string* key,
                    std::string* value);
    
private:
    std::ifstream file_;
    uint32_t block_offset_;
    std::vector<char> buffer_;
};
```

### 3.4 WAL 管理策略

- **文件大小限制**: 当 WAL 文件超过阈值（如 4MB）时，创建新的 WAL 文件
- **文件轮转**: 旧 WAL 文件在 MemTable 刷新到 SSTable 后可以删除
- **同步策略**: 
  - `WriteOptions::sync = false`: 异步写入（默认）
  - `WriteOptions::sync = true`: 同步写入（强制刷盘）

## 4. MemTable 设计

### 4.1 MemTable 的作用

- **快速写入**: 内存中的有序表，写入速度快
- **快速读取**: 支持 O(log n) 的查找
- **临时存储**: 作为写入缓冲区

### 4.2 MemTable 实现

#### 数据结构

```cpp
class MemTable {
public:
    MemTable();
    ~MemTable();
    
    void Put(const std::string& key, const std::string& value);
    bool Get(const std::string& key, std::string* value) const;
    void Delete(const std::string& key);
    
    // 转换为 SSTable
    Status FlushToSSTable(const std::string& filename);
    
    // 获取大小（用于判断是否需要刷新）
    size_t ApproximateSize() const;
    
    // 创建迭代器
    MemTableIterator* NewIterator() const;
    
private:
    // 使用跳表 (SkipList) 或 std::map 实现有序存储
    std::map<std::string, std::string> table_;
    size_t size_;
    static const size_t kMaxSize = 4 * 1024 * 1024;  // 4MB
};
```

#### 内存管理

- **大小限制**: 默认 4MB，超过后转换为 Immutable MemTable
- **内存估算**: 使用 key + value 长度估算，而非精确计算
- **双缓冲**: MemTable + Immutable MemTable，确保写入不阻塞

## 5. SSTable 设计

### 5.1 SSTable 的作用

- **持久化存储**: 磁盘上的不可变数据文件
- **有序存储**: 按键排序，支持快速查找
- **压缩存储**: 减少磁盘空间占用

### 5.2 SSTable 文件格式

#### 文件结构

```
SSTable File: <db_name>/<level>/<file_number>.sst

┌─────────────────────────────────────────────────────────┐
│                    Data Blocks                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐ │
│  │ Block 0  │  │ Block 1  │  │ Block 2  │  │ Block N│ │
│  └──────────┘  └──────────┘  └──────────┘  └────────┘ │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│                 Index Block                              │
│  (指向每个 Data Block 的起始 key 和偏移量)                │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│                 Meta Block (可选)                        │
│  (Bloom Filter, 统计信息等)                              │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│                    Footer                                │
│  (Index Block 偏移, Meta Block 偏移, Magic Number)       │
└─────────────────────────────────────────────────────────┘
```

#### Data Block 格式

```
Data Block:
┌─────────────┬─────────────┬─────────────┬─────────────┐
│ Entry 0     │ Entry 1     │ Entry 2     │ ...         │
│ (key+value) │ (key+value) │ (key+value) │             │
└─────────────┴─────────────┴─────────────┴─────────────┘
┌─────────────────────────────────────────────────────────┐
│ Restart Points (每 16 个 entry 一个重启点)                │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Block Trailer (Compression Type + Checksum)              │
└─────────────────────────────────────────────────────────┘
```

#### Entry 格式 (共享前缀压缩)

```
Entry Format:
┌──────────────┬──────────────┬──────────────┬──────────────┐
│ Shared Len    │ Non-Shared   │ Value Length │ Non-Shared   │
│  (varint)    │  Key Length   │  (varint)    │     Key      │
│              │  (varint)     │              │ (variable)   │
├──────────────┼──────────────┼──────────────┼──────────────┤
│     Value    │               │              │              │
│  (variable)  │               │              │              │
└──────────────┴──────────────┴──────────────┴──────────────┘

示例:
Entry 0: key="apple", value="red"
Entry 1: key="application", value="software"
  - Shared Len: 3 ("app")
  - Non-Shared Key: "lication"
  - Value: "software"
```

#### Footer 格式

```
Footer (固定 48 字节):
┌─────────────────────────────────────────────────────────┐
│ Index Block Handle (offset + size, 16 bytes)            │
├─────────────────────────────────────────────────────────┤
│ Meta Block Handle (offset + size, 16 bytes)              │
├─────────────────────────────────────────────────────────┤
│ Padding (8 bytes)                                        │
├─────────────────────────────────────────────────────────┤
│ Magic Number (8 bytes) = 0xdb4775248b80fb57             │
└─────────────────────────────────────────────────────────┘
```

### 5.3 SSTable 实现

#### SSTable Builder

```cpp
class SSTableBuilder {
public:
    SSTableBuilder(const std::string& filename);
    ~SSTableBuilder();
    
    void Add(const std::string& key, const std::string& value);
    Status Finish();  // 完成写入，写入 Index 和 Footer
    
private:
    void FlushBlock();  // 刷新当前 Data Block
    void WriteBlock(const Block& block, BlockHandle* handle);
    
    std::ofstream file_;
    Block current_block_;
    std::vector<BlockHandle> index_entries_;
    std::string last_key_;
    size_t block_count_;
};
```

#### SSTable Reader

```cpp
class SSTableReader {
public:
    SSTableReader(const std::string& filename);
    ~SSTableReader();
    
    Status Open();
    Status Get(const std::string& key, std::string* value);
    Iterator* NewIterator();
    
private:
    Status ReadFooter();
    Status ReadIndex();
    Status ReadBlock(const BlockHandle& handle, Block* block);
    
    std::ifstream file_;
    Footer footer_;
    IndexBlock index_;
    std::string filename_;
};
```

### 5.4 SSTable 查找算法

```
1. 读取 Footer，获取 Index Block 位置
2. 读取 Index Block
3. 在 Index Block 中二分查找，找到包含目标 key 的 Data Block
4. 读取 Data Block
5. 在 Data Block 中查找（使用重启点优化）
6. 返回结果
```

## 6. Manifest 和 Version 管理

### 6.1 Manifest 的作用

- **元数据管理**: 记录所有 SSTable 文件的信息
- **版本控制**: 支持多版本快照
- **恢复信息**: 数据库重启时恢复文件列表

### 6.2 Manifest 文件格式

```
Manifest File: <db_name>/MANIFEST-<version>

Format: 一系列 VersionEdit 记录

VersionEdit:
┌──────────────┬──────────────┬──────────────┐
│  Comparator  │  Log Number  │  Next File   │
│  (string)    │   (uint64)   │  Number      │
├──────────────┼──────────────┼──────────────┤
│  Last Seq    │  Level 0     │  Level 1     │
│  (uint64)    │  Files       │  Files       │
│              │  (repeated)  │  (repeated)  │
└──────────────┴──────────────┴──────────────┘
```

### 6.3 Version 和 VersionSet

```cpp
class Version {
public:
    // 获取指定 key 的值
    Status Get(const ReadOptions& options,
               const std::string& key,
               std::string* value);
    
    // 添加文件到指定 level
    void AddFile(int level, const FileMetaData& file);
    
    // 删除文件
    void RemoveFile(int level, uint64_t file_number);
    
    // 引用计数管理（用于多线程读取）
    void Ref() { refs_++; }
    void Unref() { 
        refs_--; 
        if (refs_ == 0) delete this;
    }
    
private:
    std::vector<std::vector<FileMetaData>> files_;  // files_[level][file_index]
    VersionSet* vset_;
    std::atomic<int> refs_;  // 引用计数，支持并发读取
    uint64_t sequence_number_;  // 版本序列号
};

class VersionSet {
public:
    VersionSet(const std::string& dbname, const Options& options);
    ~VersionSet();
    
    // 创建新版本
    Version* NewVersion();
    
    // 应用 VersionEdit，创建新版本
    Status LogAndApply(VersionEdit* edit, Version** new_version);
    
    // 恢复 Manifest
    Status Recover();
    
    // 获取当前版本（增加引用计数）
    Version* current() const { 
        current_->Ref();
        return current_; 
    }
    
    // 获取指定序列号的版本（用于快照）
    Version* GetVersion(uint64_t sequence) const;
    
    // 获取下一个序列号
    uint64_t NextSequenceNumber() {
        return sequence_number_.fetch_add(1) + 1;
    }
    
private:
    std::string dbname_;
    Version* current_;
    uint64_t next_file_number_;
    uint64_t manifest_file_number_;
    std::string manifest_filename_;
    std::atomic<uint64_t> sequence_number_;  // 全局序列号
    std::map<uint64_t, Version*> version_cache_;  // 版本缓存（用于快照）
};
```

## 7. 数据恢复机制

### 7.1 恢复流程

```
数据库启动时:

1. 读取 CURRENT 文件
   └─► 获取当前 Manifest 文件名
   
2. 读取 Manifest
   └─► 恢复所有 SSTable 文件列表
   └─► 恢复版本信息
   
3. 读取最新的 WAL 文件
   └─► 重放所有日志记录到 MemTable
   
4. 如果 MemTable 过大
   └─► 立即刷新到 SSTable
   
5. 数据库就绪
```

### 7.2 CURRENT 文件

```
CURRENT File: <db_name>/CURRENT
Content: MANIFEST-000001\n

作用: 指向当前使用的 Manifest 文件
```

### 7.3 恢复实现

```cpp
Status DB::Recover() {
    // 1. 读取 CURRENT
    std::string current;
    Status s = ReadFileToString(CurrentFileName(dbname_), &current);
    if (!s.ok()) {
        // 首次创建数据库
        return Status::OK();
    }
    
    // 2. 读取 Manifest
    std::string manifest_file = TrimSpace(current);
    s = version_set_->Recover(manifest_file);
    if (!s.ok()) return s;
    
    // 3. 查找最新的 WAL 文件
    std::vector<std::string> log_files;
    s = GetLogFiles(&log_files);
    if (!s.ok()) return s;
    
    // 4. 重放 WAL
    for (const auto& log_file : log_files) {
        WALReader reader(log_file);
        s = reader.Replay(memtable_);
        if (!s.ok()) return s;
    }
    
    // 5. 如果 MemTable 过大，立即刷新
    if (memtable_->ApproximateSize() > options_.write_buffer_size) {
        s = CompactMemTable();
        if (!s.ok()) return s;
    }
    
    return Status::OK();
}
```

## 8. Compaction 策略

### 8.1 Compaction 的目的

- **减少文件数量**: 合并多个小文件
- **删除过期数据**: 删除被覆盖或已删除的 key
- **优化读取性能**: 减少需要查找的文件数量
- **控制文件大小**: 保持合理的文件大小分布

### 8.2 Leveled Compaction

#### Level 结构

```
Level 0: 多个小文件（可能有重叠的 key 范围）
Level 1: 较大文件，key 范围不重叠
Level 2: 更大文件，key 范围不重叠
...
Level N: 最大文件，key 范围不重叠
```

#### Compaction 触发条件

1. **Level 0**: 文件数量超过阈值（如 4 个文件）
2. **Level N (N>0)**: 总大小超过阈值
   - Level 1: 10MB
   - Level 2: 100MB
   - Level 3: 1GB
   - ...

#### Compaction 过程

```
Level 0 Compaction:
1. 选择所有 Level 0 文件
2. 选择 Level 1 中与 Level 0 有重叠的文件
3. 合并所有文件，生成新的 Level 1 文件
4. 删除旧文件，更新 Manifest

Level N Compaction (N > 0):
1. 选择 Level N 的一个文件
2. 选择 Level N+1 中与 Level N 文件有重叠的文件
3. 合并文件，生成新的 Level N+1 文件
4. 删除旧文件，更新 Manifest
```

### 8.3 Compaction 实现

```cpp
class Compactor {
public:
    Compactor(VersionSet* vset, const Options& options);
    ~Compactor();
    
    // 执行 Compaction
    Status DoCompaction();
    
    // 检查是否需要 Compaction
    bool NeedsCompaction() const;
    
private:
    // 选择要 Compaction 的文件
    void PickFilesToCompact();
    
    // 合并文件
    Status MergeFiles(const std::vector<FileMetaData>& inputs,
                      std::vector<FileMetaData>* outputs);
    
    VersionSet* vset_;
    Options options_;
    CompactionState* compaction_state_;
};
```

## 9. 文件命名规范

### 9.1 文件类型和命名

```
<db_name>/
├── CURRENT              # 当前 Manifest 文件名
├── LOCK                # 数据库锁文件
├── LOG                 # 当前 WAL 文件
├── LOG.old             # 旧的 WAL 文件
├── MANIFEST-000001     # Manifest 文件
├── 0/                  # Level 0 目录
│   ├── 000001.sst      # SSTable 文件
│   ├── 000002.sst
│   └── ...
├── 1/                  # Level 1 目录
│   ├── 000010.sst
│   └── ...
└── ...
```

### 9.2 文件编号管理

- **全局文件编号**: 每个文件（WAL、SSTable、Manifest）都有唯一编号
- **递增分配**: 文件编号单调递增
- **持久化**: 文件编号保存在 Manifest 中

## 10. 性能优化

### 10.1 写入优化

- **批量写入**: WriteBatch 原子性写入
- **异步刷新**: MemTable 异步刷新到 SSTable
- **顺序写入**: WAL 和 SSTable 都是顺序写入

### 10.2 读取优化

- **Bloom Filter**: 快速判断 key 是否不存在
- **Block Cache**: 缓存常用的 Data Block
- **Index 缓存**: 缓存 Index Block
- **预取**: 预取相邻的 Block

### 10.3 空间优化

- **共享前缀压缩**: Data Block 中的 key 压缩
- **Compaction**: 定期合并文件，删除过期数据
- **SSTable 压缩**: 可选的数据压缩算法（Snappy、Zlib）

## 11. 读性能 Scale Out 和一致性视图

### 11.1 读性能 Scale Out 策略

#### 11.1.1 多版本并发控制 (MVCC)

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

#### 11.1.2 版本引用计数管理

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

#### 11.1.3 读取路径优化

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

#### 11.1.4 读取并发度提升

**设计要点**:

- **无锁数据结构**: MemTable 使用无锁数据结构（如无锁跳表）
- **文件句柄池**: 复用文件句柄，减少文件打开/关闭开销
- **Block Cache 共享**: 多个读取线程共享 Block Cache
- **预取优化**: 根据访问模式预取相邻 Block

**性能指标**:
- **读取吞吐量**: 目标支持 100K+ QPS（单机）
- **读取延迟**: P99 延迟 < 1ms（缓存命中）
- **并发度**: 支持 1000+ 并发读取线程

### 11.2 一致性视图设计

#### 11.2.1 快照隔离 (Snapshot Isolation)

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

#### 11.2.2 序列号管理

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

#### 11.2.3 版本可见性判断

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

#### 11.2.4 快照生命周期管理

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

#### 11.2.5 一致性保证级别

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

#### 11.2.6 并发控制机制

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

### 11.3 读性能 Scale Out 架构

#### 11.3.1 单机多线程读取

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

#### 11.3.2 读取路径性能优化

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

#### 11.3.3 一致性视图的性能影响

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

### 11.4 一致性视图的使用场景

#### 11.4.1 事务性读取

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

#### 11.4.2 报表和统计

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

#### 11.4.3 时间点恢复

```cpp
// 恢复到指定时间点的数据
uint64_t target_sequence = GetSequenceForTimestamp(timestamp);
const Snapshot* recovery_snapshot = CreateSnapshot(target_sequence);

// 使用恢复快照读取数据
ReadOptions options;
options.snapshot = recovery_snapshot;
// ... 读取操作
```

## 12. 错误处理和一致性

### 12.1 校验和

- **WAL 记录校验**: 每条记录都有 CRC32 校验和
- **SSTable Block 校验**: 每个 Block 都有校验和
- **Manifest 校验**: Manifest 文件也有校验和

### 12.2 原子性操作

- **文件重命名**: 使用原子重命名操作
- **CURRENT 更新**: 先写临时文件，再原子重命名
- **Compaction**: 先写新文件，再删除旧文件，最后更新 Manifest

### 12.3 崩溃恢复

- **WAL 重放**: 从 WAL 恢复未刷新的数据
- **部分写入检测**: 通过校验和检测损坏的文件
- **文件清理**: 启动时清理临时文件和损坏文件

## 13. 实现计划

### 13.1 第一阶段：基础 WAL

- [ ] 实现 WALWriter 和 WALReader
- [ ] 集成 WAL 到 DB::Put/Delete
- [ ] 实现数据库启动时的 WAL 重放
- [ ] 测试 WAL 的基本功能

### 13.2 第二阶段：MemTable

- [ ] 实现 MemTable（使用 std::map 或跳表）
- [ ] 实现 MemTable 到 SSTable 的刷新
- [ ] 实现双缓冲（MemTable + Immutable MemTable）
- [ ] 测试 MemTable 功能

### 13.3 第三阶段：SSTable

- [ ] 实现 SSTableBuilder
- [ ] 实现 SSTableReader
- [ ] 实现 Block 格式和压缩
- [ ] 实现 Index Block 和 Footer
- [ ] 测试 SSTable 读写

### 13.4 第四阶段：Version 管理

- [ ] 实现 Version 和 VersionSet
- [ ] 实现 Manifest 读写
- [ ] 实现文件管理（CURRENT、LOCK）
- [ ] 测试版本管理

### 13.5 第五阶段：Compaction

- [ ] 实现 Compaction 策略
- [ ] 实现 Level 0 到 Level 1 的 Compaction
- [ ] 实现多级 Compaction
- [ ] 测试 Compaction 功能

### 13.6 第六阶段：优化和测试

- [ ] 实现 Bloom Filter
- [ ] 实现 Block Cache
- [ ] 性能测试和优化
- [ ] 压力测试和稳定性测试

### 13.7 第七阶段：读性能 Scale Out 和一致性视图

- [ ] 实现序列号管理机制
- [ ] 实现版本引用计数管理
- [ ] 实现快照（Snapshot）机制
- [ ] 实现版本可见性判断逻辑
- [ ] 实现无锁读取路径
- [ ] 实现版本缓存和清理机制
- [ ] 实现并行 SSTable 查找
- [ ] 测试并发读取性能
- [ ] 测试一致性视图正确性
- [ ] 性能基准测试（吞吐量、延迟、并发度）

## 14. 接口变更

### 14.1 DB::Open 变更

```cpp
// 当前实现
Status DB::Open(const Options& options, const std::string& name, DB** dbptr);

// 持久化实现需要:
// 1. 创建/打开数据库目录
// 2. 读取 LOCK 文件（防止多进程访问）
// 3. 恢复数据（读取 Manifest 和 WAL）
// 4. 初始化 MemTable
```

### 14.2 Options 扩展

```cpp
struct Options {
    // 现有选项...
    
    // 新增持久化相关选项
    size_t write_buffer_size;      // MemTable 大小限制（默认 4MB）
    size_t max_file_size;          // SSTable 文件大小限制（默认 2MB）
    int max_open_files;            // 最大打开文件数（默认 1000）
    CompressionType compression;   // 压缩类型（默认 kNoCompression）
    bool create_if_missing;        // 数据库不存在时创建（已存在）
    bool error_if_exists;          // 数据库存在时报错（已存在）
    
    // 读性能 Scale Out 相关选项
    size_t block_cache_size;       // Block Cache 大小（默认 8MB）
    size_t version_cache_size;     // 版本缓存大小（默认 100）
    int max_read_threads;          // 最大读取线程数（默认 CPU 核心数）
};

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

## 15. 测试策略

### 15.1 单元测试

- WAL 读写测试
- MemTable 操作测试
- SSTable 构建和读取测试
- Compaction 测试

### 15.2 集成测试

- 完整的写入-读取-恢复流程
- 崩溃恢复测试
- 并发写入测试
- 大数据量测试

### 15.3 性能测试

- 写入吞吐量测试
- 读取延迟测试
- Compaction 性能测试
- 空间占用测试

## 16. 参考设计

本设计参考了以下存储系统的实现：

- **LevelDB**: Google 的键值存储库，本设计的核心参考
- **RocksDB**: Facebook 基于 LevelDB 的优化版本
- **Bitcask**: Riak 的日志结构存储引擎

## 17. 总结

本设计文档描述了基于 WAL + SSTable 的持久化存储实现方案。该方案提供了：

- **数据持久化**: 通过 WAL 和 SSTable 实现数据持久化
- **高性能**: 顺序写入、有序存储、快速查找
- **可靠性**: 校验和、原子操作、崩溃恢复
- **可扩展性**: 支持数据增长和 Compaction

实现将分阶段进行，确保每个阶段都有完整的测试覆盖。
