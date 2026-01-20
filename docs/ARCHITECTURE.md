# KV存储引擎设计文档

## 1. 概述

本文档描述了一个LevelDB风格的键值存储引擎的完整设计，包括高层架构设计和底层实现细节。

### 1.1 设计目标

- **接口兼容性**: 遵循LevelDB的标准API设计，便于用户迁移
- **简单可靠**: 提供稳定、易用的KV存储接口
- **可扩展性**: 支持从内存实现扩展到持久化存储
- **高性能**: 优化读写性能，支持批量操作

### 1.2 技术选型

- **语言**: C++17
- **构建系统**: CMake 3.10+
- **测试框架**: Google Test
- **日志系统**: glog (预留)
- **序列化**: Protocol Buffers (预留)

## 2. High Level Design (高层设计)

### 2.1 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│  (User Code using DB, Status, Options, Iterator APIs)   │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                    Interface Layer                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐│
│  │    DB    │  │  Status  │  │ Options  │  │Iterator ││
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘│
│  ┌──────────┐  ┌──────────┐                             │
│  │WriteBatch│  │DBIterator│                             │
│  └──────────┘  └──────────┘                             │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                   Storage Layer                          │
│  ┌──────────────────────────────────────────────────┐  │
│  │         In-Memory Storage (Current)               │  │
│  │    std::unordered_map<std::string, std::string>   │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │      Persistent Storage (Future Extension)        │  │
│  │  WAL + SSTable + LSM-Tree + Compaction           │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 2.2 模块划分

#### 2.2.1 核心接口模块

**DB类** - 主数据库接口
- **职责**: 提供数据库的打开、关闭和基本CRUD操作
- **接口**: Open, Put, Get, Delete, Write, NewIterator
- **设计模式**: Factory模式（Open静态方法）

**Status类** - 错误处理
- **职责**: 统一的操作结果和错误信息表示
- **设计**: 值语义，轻量级，无异常抛出
- **状态类型**: OK, NotFound, Corruption, InvalidArgument, IOError, NotSupported

**Options类族** - 配置管理
- **Options**: 数据库级别配置（create_if_missing, error_if_exists等）
- **ReadOptions**: 读取操作配置（verify_checksums, fill_cache, snapshot）
- **WriteOptions**: 写入操作配置（sync）

#### 2.2.2 数据访问模块

**Iterator接口** - 数据遍历抽象
- **职责**: 提供统一的数据遍历接口
- **操作**: Seek, SeekToFirst, SeekToLast, Next, Prev
- **设计模式**: Iterator模式

**DBIterator类** - 迭代器实现
- **职责**: 基于内存数据结构的迭代器实现
- **特性**: 按键排序遍历，支持双向移动

**WriteBatch类** - 批量操作
- **职责**: 原子性批量写入操作
- **设计**: 命令模式，支持Put和Delete操作的批量执行

### 2.3 接口设计原则

1. **一致性**: 所有操作返回Status对象
2. **资源管理**: RAII原则，自动资源清理
3. **不可变性**: Options对象在创建后不可修改（通过构造函数初始化）
4. **扩展性**: 接口抽象，便于实现不同的存储后端

### 2.4 数据流设计

#### 写入流程
```
User Code
    │
    ├─► WriteBatch.Put/Delete (构建批量操作)
    │
    └─► DB.Write(WriteBatch) ──► Storage Layer (原子性写入)
```

#### 读取流程
```
User Code
    │
    ├─► DB.Get(ReadOptions, key) ──► Storage Layer ──► Return Status + Value
    │
    └─► DB.NewIterator ──► DBIterator ──► Storage Layer ──► Iterate Results
```

## 3. Low Level Design (底层设计)

### 3.1 数据结构设计

#### 3.1.1 当前实现（内存版本）

**DB类内部存储**
```cpp
class DB {
private:
    std::unordered_map<std::string, std::string> data_store_;
    // Key: std::string (用户键)
    // Value: std::string (用户值)
};
```

**选择unordered_map的原因**:
- O(1)平均时间复杂度的查找和插入
- 适合内存KV存储场景
- 简单直接，易于实现和调试

**WriteBatch内部结构**
```cpp
struct WriteBatch::Rep {
    std::vector<std::pair<std::string, std::string>> puts;    // Put操作列表
    std::vector<std::string> deletes;                          // Delete操作列表
};
```

**DBIterator内部结构**
```cpp
class DBIterator {
private:
    std::vector<std::pair<std::string, std::string>> sorted_data_;  // 排序后的数据快照
    size_t current_index_;                                           // 当前索引位置
    Status status_;                                                  // 迭代器状态
};
```

**设计考虑**:
- 使用vector存储排序后的数据，支持O(log n)的Seek操作
- 在构造时创建快照，保证迭代器的一致性视图
- 使用索引而非迭代器，简化边界检查

#### 3.1.2 Status类实现

```cpp
class Status {
private:
    enum Code {
        kOk = 0,
        kNotFound = 1,
        kCorruption = 2,
        kNotSupported = 3,
        kInvalidArgument = 4,
        kIOError = 5
    };
    Code code_;
    std::string msg_;
};
```

**设计要点**:
- 值语义，可复制，无动态分配（成功时）
- 错误信息仅在非OK状态时存储，节省内存
- 提供类型检查方法（IsNotFound, IsCorruption等）

### 3.2 算法设计

#### 3.2.1 Put操作
```
Algorithm: Put(key, value)
1. 直接插入或更新 unordered_map
2. 时间复杂度: O(1) 平均，O(n) 最坏（哈希冲突）
3. 空间复杂度: O(1)
```

#### 3.2.2 Get操作
```
Algorithm: Get(key, value)
1. 在 unordered_map 中查找 key
2. 如果找到，复制value到输出参数
3. 返回相应的Status（OK或NotFound）
4. 时间复杂度: O(1) 平均，O(n) 最坏
```

#### 3.2.3 Delete操作
```
Algorithm: Delete(key)
1. 从 unordered_map 中删除 key（如果存在）
2. 总是返回OK（删除不存在的key不算错误）
3. 时间复杂度: O(1) 平均，O(n) 最坏
```

#### 3.2.4 WriteBatch执行
```
Algorithm: Write(WriteBatch)
1. 遍历WriteBatch中的所有Put操作，执行Put
2. 遍历WriteBatch中的所有Delete操作，执行Delete
3. 保证原子性（当前实现中所有操作顺序执行）
4. 时间复杂度: O(n)，n为批量操作数量
```

#### 3.2.5 Iterator实现
```
Algorithm: DBIterator构造
1. 将unordered_map转换为vector
2. 对vector按键排序（std::sort）
3. 时间复杂度: O(n log n)

Algorithm: Seek(target)
1. 使用std::lower_bound二分查找
2. 时间复杂度: O(log n)

Algorithm: Next/Prev
1. 递增/递减索引
2. 时间复杂度: O(1)
```

### 3.3 内存管理

#### 3.3.1 对象生命周期

**DB对象**
- 用户通过`DB::Open`创建，通过`delete`释放
- 使用原始指针，遵循LevelDB接口风格

**Iterator对象**
- 通过`DB::NewIterator`创建，用户负责`delete`
- 必须在DB对象销毁前释放

**WriteBatch对象**
- 栈对象，自动管理生命周期
- 内部使用堆分配的Rep结构

#### 3.3.2 内存优化

**Status对象**
- 成功时：仅存储枚举值（8字节）
- 失败时：额外存储错误消息字符串

**DBIterator对象**
- 构造时创建数据快照，占用O(n)额外空间
- 适合只读遍历场景，不适合频繁创建

### 3.4 线程安全

**当前实现**: 非线程安全
- 所有操作假设单线程访问
- 无锁设计，无同步原语

**未来扩展方向**:
- 读写锁（ReadWriteLock）保护data_store_
- 无锁数据结构（Lock-free hash table）
- 分片（Sharding）减少锁竞争

### 3.5 错误处理

#### 3.5.1 Status使用模式

```cpp
// 模式1: 检查ok()
Status status = db->Get(read_options, key, &value);
if (!status.ok()) {
    // 处理错误
}

// 模式2: 检查特定错误类型
if (status.IsNotFound()) {
    // 键不存在，特殊处理
}

// 模式3: 获取错误信息
std::cerr << "Error: " << status.ToString() << std::endl;
```

#### 3.5.2 错误传播

- 所有DB操作返回Status
- Iterator操作通过status()方法返回错误
- 不抛出异常，完全基于返回值

### 3.6 性能优化

#### 3.6.1 当前实现的性能特征

| 操作 | 时间复杂度 | 空间复杂度 | 说明 |
|------|-----------|-----------|------|
| Put | O(1) 平均 | O(1) | 哈希表插入 |
| Get | O(1) 平均 | O(1) | 哈希表查找 |
| Delete | O(1) 平均 | O(1) | 哈希表删除 |
| WriteBatch | O(n) | O(n) | n为批量操作数 |
| Iterator构造 | O(n log n) | O(n) | 需要排序 |
| Iterator.Seek | O(log n) | O(1) | 二分查找 |
| Iterator.Next | O(1) | O(1) | 索引递增 |

#### 3.6.2 优化策略

**短期优化**:
1. **字符串优化**: 使用string_view减少拷贝
2. **预分配**: WriteBatch预分配vector容量
3. **移动语义**: 使用std::move减少拷贝

**长期优化**:
1. **持久化**: WAL + SSTable，减少内存占用
2. **压缩**: 键值压缩，减少存储空间
3. **缓存**: LRU缓存热点数据
4. **并发**: 读写锁或无锁结构

### 3.7 扩展点设计

#### 3.7.1 存储后端抽象

当前设计便于扩展不同的存储后端：

```cpp
class StorageBackend {
public:
    virtual Status Put(const std::string& key, const std::string& value) = 0;
    virtual Status Get(const std::string& key, std::string* value) = 0;
    virtual Status Delete(const std::string& key) = 0;
    virtual Iterator* NewIterator() = 0;
};

// 当前实现
class MemoryStorage : public StorageBackend { ... };

// 未来实现
class PersistentStorage : public StorageBackend { ... };
```

#### 3.7.2 持久化扩展路径

1. **WAL层**: 写入前先写日志
2. **SSTable层**: 不可变数据文件
3. **LSM-Tree层**: 多级存储结构
4. **Compaction层**: 数据合并和压缩

## 4. 设计决策记录

### 4.1 为什么使用unordered_map而不是map？

**决策**: 使用`std::unordered_map`

**理由**:
- O(1)平均时间复杂度 vs O(log n)
- 内存KV存储场景不需要有序性
- Iterator需要有序时，在构造时排序即可

**权衡**:
- 失去自动排序，但Iterator可以按需排序
- 哈希冲突可能导致性能下降，但实际场景中影响小

### 4.2 为什么使用Status而不是异常？

**决策**: 使用Status返回值

**理由**:
- 符合LevelDB接口风格
- 性能更好（无异常开销）
- 错误处理更明确
- 适合系统编程场景

### 4.3 为什么Iterator需要创建快照？

**决策**: Iterator构造时创建数据快照

**理由**:
- 保证迭代器视图的一致性
- 避免迭代过程中数据修改导致的问题
- 简化并发控制（未来扩展）

**权衡**:
- 增加内存开销O(n)
- 适合只读遍历，不适合频繁创建

## 5. 未来扩展设计

### 5.1 持久化存储设计

```
┌─────────────────┐
│   User Write    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   WAL (Log)     │ ◄─── 崩溃恢复
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  MemTable       │ ◄─── 内存表
└────────┬────────┘
         │ (flush when full)
         ▼
┌─────────────────┐
│   SSTable       │ ◄─── 磁盘文件
└─────────────────┘
```

### 5.2 LSM-Tree结构

```
Level 0: 多个SSTable（可能有重叠）
Level 1: 合并后的SSTable（无重叠）
Level 2: 更大的SSTable
...
```

### 5.3 Compaction策略

- **Size-tiered**: 相同大小的SSTable合并
- **Leveled**: 每层一个SSTable，按大小分层

## 6. 测试设计

### 6.1 单元测试覆盖

- **Status类**: 所有状态类型和检查方法
- **Options类**: 默认值和配置
- **WriteBatch类**: Put、Delete、Clear、Iterate
- **DB类**: Open、Put、Get、Delete、Write
- **Iterator类**: Seek、Next、Prev、边界情况

### 6.2 集成测试

- **批量操作**: 大量数据的WriteBatch
- **迭代器**: 大数据集的遍历性能
- **错误处理**: 各种错误场景

### 6.3 性能测试

- **吞吐量**: Put/Get操作的QPS
- **延迟**: 单次操作延迟
- **内存**: 大数据集的内存占用

## 7. 参考资料

- [LevelDB Architecture](https://github.com/google/leveldb/blob/main/doc/impl.md)
- [LSM-Tree Paper](https://www.cs.umb.edu/~poneil/lsmtree.pdf)
- [The Log-Structured Merge-Tree](https://www.cse.ust.hk/~dimitris/courses/cse690g-fall07/reading/LSM.pdf)
- [C++17 Standard](https://en.cppreference.com/w/cpp/17)
