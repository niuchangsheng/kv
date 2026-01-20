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
│  (详细格式见下方 Index Block 格式说明)                    │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│                 Meta Block (可选)                        │
│  (Bloom Filter, 统计信息等)                              │
│  (详细格式见下方 Meta Block 格式说明)                     │
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
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐  │
│  │ Offset 0 │  │ Offset 1 │  │ Offset 2 │  │Offset N│  │
│  │ (4 bytes)│  │ (4 bytes)│  │ (4 bytes)│  │(4 bytes)│  │
│  └──────────┘  └──────────┘  └──────────┘  └────────┘  │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Restart Point Count (4 bytes)                           │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Block Trailer (Compression Type + Checksum)              │
│  ┌──────────────┬────────────────────────────────────┐  │
│  │ Compression  │ Checksum (CRC32, 4 bytes)         │  │
│  │ Type (1 byte)│                                    │  │
│  └──────────────┴────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

**Restart Points (重启点) 设计**:

**用途**:
1. **快速定位**: 无需从 Block 开头顺序解码，可以直接跳到重启点位置
2. **二分查找优化**: 在重启点之间进行二分查找，提高查找效率
3. **减少解码开销**: 重启点处的 Entry 存储完整 key（shared_len = 0），无需依赖前一个 key

**工作原理**:
- **重启点间隔**: 默认每 16 个 Entry 设置一个重启点（可配置）
- **重启点 Entry**: 在重启点位置的 Entry，`shared_len = 0`，存储完整的 key
- **重启点数组**: 存储每个重启点在 Block 内的字节偏移量（相对于 Block 起始位置）
- **重启点数量**: 存储在 Block 末尾，用于确定重启点数组的大小

**查找算法**:
```
1. 读取重启点数组和数量
2. 在重启点之间进行二分查找，找到目标 key 可能所在的范围
3. 从该重启点开始顺序解码 Entry，直到找到目标 key 或超出范围
```

**示例**:
```
Data Block 内容:
Entry 0:  key="apple",     shared=0, offset=0
Entry 1:  key="application", shared=3, offset=15
Entry 2:  key="apply",      shared=3, offset=35
...
Entry 15: key="banana",    shared=0, offset=200  ← 重启点 1
Entry 16: key="band",       shared=5, offset=215
...
Entry 31: key="cherry",    shared=0, offset=400  ← 重启点 2
...

重启点数组:
[0, 200, 400, ...]  (每个重启点在 Block 中的偏移量)
重启点数量: 3

查找 "banana":
1. 二分查找重启点数组，找到重启点 1 (offset=200) 包含 "banana"
2. 从 offset=200 开始顺序解码，Entry 15 就是 "banana"
```

**性能优势**:
- **顺序查找**: O(n) → **重启点优化**: O(log(restart_count)) + O(restart_interval)
- **典型场景**: 16 个重启点，每个间隔 16 个 Entry
  - 顺序查找: 平均 128 次解码
  - 重启点优化: log2(16) = 4 次重启点比较 + 平均 8 次 Entry 解码 = 12 次操作
  - **性能提升**: 约 10 倍

**Block Trailer 格式**:
```
┌──────────────┬────────────────────────────────────┐
│ Compression  │ Checksum (CRC32, 4 bytes)         │
│ Type (1 byte)│                                    │
└──────────────┴────────────────────────────────────┘

Compression Type:
  0x00 = kNoCompression
  0x01 = kSnappyCompression
  0x02 = kZlibCompression

Checksum:
  - 对整个 Block（包括所有 Entry、重启点数组、重启点数量）计算 CRC32
  - 用于检测数据损坏
```

#### Entry 格式 (共享前缀压缩)

**Entry 格式说明**:

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
```

**字段说明**:

1. **Shared Len (共享长度)**: 
   - **类型**: varint (变长整数，1-5 字节)
   - **用途**: 表示当前 key 与前一个 key 的共享前缀长度
   - **优势**: 
     - 减少存储空间：相邻 key 通常有共同前缀（如 "user:001", "user:002"）
     - 提高压缩率：对于有序数据，共享前缀非常常见
     - 降低 I/O：更小的数据块意味着更少的磁盘读取
   - **示例**: 
     ```
     Entry 0: key="apple", value="red"
     Entry 1: key="application", value="software"
       - Shared Len: 3 (前 3 个字符 "app" 是共享的)
       - Non-Shared Key Length: 8 ("lication" 的长度)
       - Non-Shared Key: "lication"
       - Value: "software"
     ```

2. **Non-Shared Key Length (非共享 key 长度)**:
   - **类型**: varint
   - **用途**: 当前 key 中与前一个 key 不共享的部分的长度
   - **计算**: `current_key.length() - shared_len`

3. **Value Length (值长度)**:
   - **类型**: varint
   - **用途**: value 的字节长度

4. **Non-Shared Key (非共享 key 部分)**:
   - **类型**: 字节数组
   - **用途**: key 中与前一个 key 不同的部分
   - **完整 key 重建**: `previous_key.substr(0, shared_len) + non_shared_key`

5. **Value (值)**:
   - **类型**: 字节数组
   - **用途**: 完整的 value 数据

**压缩效果示例**:

```
未压缩存储:
Entry 0: "user:001" (8 bytes) + "value1" (6 bytes) = 14 bytes
Entry 1: "user:002" (8 bytes) + "value2" (6 bytes) = 14 bytes
总计: 28 bytes

压缩存储:
Entry 0: 0 (shared) + 8 (key) + 6 (value) = 14 bytes
Entry 1: 6 (shared "user:0") + 2 (key "02") + 6 (value) = 14 bytes
总计: 28 bytes (此例压缩效果不明显)

更好的示例:
Entry 0: "user:profile:name" (17 bytes) + "Alice" (5 bytes) = 22 bytes
Entry 1: "user:profile:age" (15 bytes) + "25" (2 bytes) = 17 bytes
未压缩总计: 39 bytes

压缩存储:
Entry 0: 0 + 17 + 5 = 22 bytes
Entry 1: 13 (shared "user:profile:") + 2 (key "ag") + 2 (value) = 16 bytes
压缩总计: 38 bytes (节省 1 byte，但实际场景中节省更多)
```

**共享前缀压缩的优势**:
- **空间节省**: 对于有序 key，通常有 50-80% 的共享前缀
- **读取性能**: 更小的数据块意味着更快的 I/O
- **缓存效率**: 更小的数据块提高缓存命中率

#### Index Block 格式

**Index Block 的作用**:
- **快速定位**: 通过二分查找快速找到包含目标 key 的 Data Block
- **减少 I/O**: 避免读取所有 Data Block，只读取相关的 Block
- **范围查询**: 支持范围查询，快速确定需要读取的 Block 范围

**Index Block 格式**:

```
Index Block (与 Data Block 格式相同):
┌─────────────┬─────────────┬─────────────┬─────────────┐
│ Entry 0     │ Entry 1     │ Entry 2     │ ...         │
│ (key+value) │ (key+value) │ (key+value) │             │
└─────────────┴─────────────┴─────────────┴─────────────┘
┌─────────────────────────────────────────────────────────┐
│ Restart Points                                          │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Restart Point Count                                      │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Block Trailer                                            │
└─────────────────────────────────────────────────────────┘
```

**Index Entry 格式**:

每个 Index Entry 对应一个 Data Block，使用与 Data Block Entry 相同的格式（共享前缀压缩）：

```
Index Entry (使用共享前缀压缩格式):
┌──────────────┬──────────────┬──────────────┬──────────────┐
│ Shared Len    │ Non-Shared   │ Value Length │ Non-Shared   │
│  (varint)    │  Key Length   │  (varint)    │     Key      │
│              │  (varint)     │              │ (variable)   │
├──────────────┼──────────────┼──────────────┼──────────────┤
│     Value    │               │              │              │
│  (BlockHandle)│               │              │              │
│  (16 bytes)  │               │              │              │
└──────────────┴──────────────┴──────────────┴──────────────┘
```

**Index Entry Value (BlockHandle) 详细格式**:

```
BlockHandle (16 bytes):
┌─────────────────────────────────────────────────────────┐
│ Offset (8 bytes, little-endian)                          │
│  ┌──────────┬──────────┬──────────┬──────────┐          │
│  │ Byte 0-1  │ Byte 2-3 │ Byte 4-5 │ Byte 6-7 │          │
│  │ (low)     │          │          │ (high)   │          │
│  └──────────┴──────────┴──────────┴──────────┘          │
├─────────────────────────────────────────────────────────┤
│ Size (8 bytes, little-endian)                            │
│  ┌──────────┬──────────┬──────────┬──────────┐          │
│  │ Byte 0-1  │ Byte 2-3 │ Byte 4-5 │ Byte 6-7 │          │
│  │ (low)     │          │          │ (high)   │          │
│  └──────────┴──────────┴──────────┴──────────┘          │
└─────────────────────────────────────────────────────────┘
```

**Index Entry 编码示例**:

```
假设有 3 个 Data Block:

Data Block 0: 最后一个 key = "banana", offset=0,   size=200
Data Block 1: 最后一个 key = "cherry", offset=200, size=180
Data Block 2: 最后一个 key = "cloud",   offset=380, size=190

Index Block Entries:

Entry 0:
  Key: "banana" (shared_len=0, 完整存储)
  Value: BlockHandle{offset=0, size=200}
    编码: [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // offset=0
           0xC8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]  // size=200

Entry 1:
  Key: "cherry" (shared_len=0, 因为与 "banana" 无共享前缀)
  Value: BlockHandle{offset=200, size=180}
    编码: [0xC8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // offset=200
           0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]  // size=180

Entry 2:
  Key: "cloud" (shared_len=0)
  Value: BlockHandle{offset=380, size=190}
    编码: [0x7C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // offset=380
           0xBE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]  // size=190
```

**Index Block 完整结构**:

```
Index Block 布局:
┌─────────────────────────────────────────────────────────┐
│ Entry 0 (key="banana", BlockHandle{0, 200})            │
│ Entry 1 (key="cherry", BlockHandle{200, 180})          │
│ Entry 2 (key="cloud", BlockHandle{380, 190})           │
│ ...                                                     │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Restart Points Array                                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │ Offset 0 │  │ Offset 1 │  │ Offset 2 │  ...         │
│  │ (4 bytes)│  │ (4 bytes)│  │ (4 bytes)│              │
│  └──────────┘  └──────────┘  └──────────┘              │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Restart Point Count (4 bytes, little-endian)             │
│  例如: 0x03 0x00 0x00 0x00 表示 3 个重启点              │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Block Trailer (5 bytes)                                   │
│  ┌──────────────┬────────────────────────────────────┐  │
│  │ Compression  │ Checksum (CRC32, 4 bytes)          │  │
│  │ Type (1 byte)│                                    │  │
│  └──────────────┴────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

**字段说明**:

1. **Key**: 
   - **内容**: 对应 Data Block 中**最后一个 Entry 的 key**（不是第一个）
   - **用途**: 用于二分查找，判断目标 key 是否在该 Data Block 中
   - **选择原因**: 
     - 如果使用第一个 key，无法判断 key 是否在 Block 末尾
     - 使用最后一个 key，可以确定：`last_key >= target_key` 时，该 Block 可能包含目标

2. **Value (BlockHandle)**:
   - **Offset (8 bytes)**: Data Block 在 SSTable 文件中的字节偏移量
   - **Size (8 bytes)**: Data Block 的字节大小
   - **用途**: 定位和读取 Data Block

**查找算法**:

```
查找 key="target" 的流程:

1. 读取 Footer，获取 Index Block 的位置
2. 读取 Index Block
3. 在 Index Block 中进行二分查找:
   - 找到最后一个 key >= "target" 的 Index Entry
   - 该 Entry 对应的 Data Block 可能包含 "target"
4. 读取对应的 Data Block
5. 在 Data Block 中查找 "target"
```

**示例**:

```
SSTable 文件结构:
Data Block 0: ["apple", "application", "apply", "banana"]
Data Block 1: ["band", "bank", "base", "cherry"]
Data Block 2: ["city", "class", "clear", "cloud"]

Index Block Entries:
Entry 0: key="banana",  BlockHandle{offset=0,   size=200}
Entry 1: key="cherry",  BlockHandle{offset=200, size=180}
Entry 2: key="cloud",   BlockHandle{offset=380, size=190}

查找 "bank":
1. 在 Index Block 中二分查找，找到 Entry 1 (key="cherry" >= "bank")
2. 读取 Data Block 1 (offset=200, size=180)
3. 在 Data Block 1 中查找 "bank"
```

**性能优势**:
- **I/O 减少**: 只需读取 1 个 Index Block + 1 个 Data Block，而不是所有 Block
- **查找效率**: Index Block 通常很小（< 4KB），可以完全加载到内存
- **缓存友好**: Index Block 可以长期缓存，提高重复查询性能

#### Meta Block 格式

**Meta Block 的作用**:
- **快速过滤**: 使用 Bloom Filter 快速判断 key 是否不存在
- **统计信息**: 存储 Block 的统计信息（可选）
- **扩展性**: 支持未来添加其他元数据

**Meta Block 类型**:

1. **Bloom Filter Meta Block** (最常见):
   - **用途**: 快速判断 key 是否不存在，避免不必要的 Block 读取
   - **完整格式**: 
     ```
     Bloom Filter Meta Block:
     ┌─────────────────────────────────────────────────────────┐
     │ Filter Data (Bloom Filter 位数组)                        │
     │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐  │
     │  │ Byte 0   │  │ Byte 1   │  │ Byte 2   │  │ ...    │  │
     │  │ (Bit 0-7)│  │ (Bit 8-15)│ │ (Bit 16-23)│ │        │  │
     │  └──────────┘  └──────────┘  └──────────┘  └────────┘  │
     │  (按字节存储，每个字节包含 8 个位)                        │
     └─────────────────────────────────────────────────────────┘
     ┌─────────────────────────────────────────────────────────┐
     │ Filter Size (4 bytes, little-endian)                      │
     │  表示 Filter Data 的字节数                                │
     │  例如: 0x00 0x10 0x00 0x00 = 4096 字节                  │
     └─────────────────────────────────────────────────────────┘
     ```
   
   - **Bloom Filter 位数组编码**:
     ```
     位数组按字节存储，每个字节包含 8 个位:
     
     Byte 0: [Bit 0] [Bit 1] [Bit 2] [Bit 3] [Bit 4] [Bit 5] [Bit 6] [Bit 7]
     Byte 1: [Bit 8] [Bit 9] [Bit 10] [Bit 11] [Bit 12] [Bit 13] [Bit 14] [Bit 15]
     ...
     
     位操作:
     - 设置位 i: byte[i/8] |= (1 << (i % 8))
     - 检查位 i: (byte[i/8] & (1 << (i % 8))) != 0
     ```
   
   - **Bloom Filter 构建算法**:
     ```
     对于每个 key:
       1. 使用 k 个不同的 Hash 函数计算 k 个哈希值
       2. 将每个哈希值对位数组大小取模，得到位位置
       3. 将对应的位设置为 1
     
     示例:
       key = "apple"
       hash1("apple") % filter_size = 1234 → 设置位 1234
       hash2("apple") % filter_size = 5678 → 设置位 5678
       ...
       hashk("apple") % filter_size = 9012 → 设置位 9012
     ```
   
   - **Bloom Filter 查询算法**:
     ```
     对于查询 key:
       1. 使用相同的 k 个 Hash 函数计算 k 个哈希值
       2. 检查对应的 k 个位是否都为 1
       3. 如果所有位都为 1，返回 "可能存在"
       4. 如果有任何位为 0，返回 "肯定不存在"
     
     示例:
       查询 "banana":
         hash1("banana") % filter_size = 2345 → 检查位 2345
         hash2("banana") % filter_size = 6789 → 检查位 6789
         ...
         如果所有位都是 1 → 可能存在（继续查找）
         如果有任何位是 0 → 肯定不存在（快速返回）
     ```
   
   - **Bloom Filter 参数**:
     - **Hash 函数数量 (k)**: 通常 10-15 个
       - 计算公式: `k = (filter_size / num_keys) * ln(2)`
       - 更多 hash 函数 → 更低的误判率，但计算开销更大
     - **位数组大小 (m)**: 根据数据量计算
       - 计算公式: `m = -n * ln(p) / (ln(2)^2)`
       - 其中 n = key 数量, p = 目标误判率
       - 通常每个 key 10 bits (误判率约 1%)
     - **误判率**: 通常 < 1%
       - 误判率 = (1 - e^(-kn/m))^k
       - 实际应用中，误判率通常控制在 0.1% - 1%
   
   - **Bloom Filter 编码示例**:
     ```
     假设有 1000 个 key，目标误判率 1%:
     
     计算参数:
       m = -1000 * ln(0.01) / (ln(2)^2) ≈ 9585 bits ≈ 1198 bytes
       k = (1198 * 8 / 1000) * ln(2) ≈ 6.6 → 取 7 个 hash 函数
     
     Filter 存储:
       Filter Data: 1198 字节的位数组
       Filter Size: 0xB6 0x04 0x00 0x00 (1198 的 little-endian)
     
     查询示例:
       查询 "apple":
         hash1("apple") % 9585 = 1234 → 检查 byte[154], bit 2
         hash2("apple") % 9585 = 5678 → 检查 byte[709], bit 6
         ...
         hash7("apple") % 9585 = 9012 → 检查 byte[1126], bit 4
     ```

2. **Statistics Meta Block** (可选):
   - **用途**: 存储 SSTable 的统计信息，用于查询优化和监控
   - **格式** (使用类似 Data Block 的格式，但存储 key-value 对):
     ```
     Statistics Meta Block:
     ┌─────────────────────────────────────────────────────────┐
     │ Entry 0: key="num_keys", value="1000000"                │
     │ Entry 1: key="data_size", value="10485760"             │
     │ Entry 2: key="index_size", value="4096"                │
     │ ...                                                     │
     └─────────────────────────────────────────────────────────┘
     ┌─────────────────────────────────────────────────────────┐
     │ Restart Points                                           │
     └─────────────────────────────────────────────────────────┘
     ┌─────────────────────────────────────────────────────────┐
     │ Restart Point Count                                      │
     └─────────────────────────────────────────────────────────┘
     ┌─────────────────────────────────────────────────────────┐
     │ Block Trailer                                            │
     └─────────────────────────────────────────────────────────┘
     ```
   
   - **Statistics Entry 格式**:
     ```
     每个统计项使用与 Data Block Entry 相同的格式:
     
     ┌──────────────┬──────────────┬──────────────┬──────────────┐
     │ Shared Len    │ Non-Shared   │ Value Length │ Non-Shared   │
     │  (varint)    │  Key Length   │  (varint)    │     Key      │
     │              │  (varint)     │              │ (variable)   │
     ├──────────────┼──────────────┼──────────────┼──────────────┤
     │     Value    │               │              │              │
     │  (string)    │               │              │              │
     │  (variable)  │               │              │              │
     └──────────────┴──────────────┴──────────────┴──────────────┘
     
     示例:
       Entry: key="num_keys", value="1000000"
       - Shared Len: 0 (第一个 entry)
       - Non-Shared Key: "num_keys" (8 bytes)
       - Value: "1000000" (7 bytes)
     ```
   
   - **统计信息内容** (标准字段):
     - `num_keys` (string): 总 key 数量，例如 "1000000"
     - `data_size` (string): Data Block 总大小（字节），例如 "10485760"
     - `index_size` (string): Index Block 大小（字节），例如 "4096"
     - `filter_size` (string): Bloom Filter 大小（字节），例如 "1198"
     - `min_key` (string): 最小 key（第一个 key），例如 "apple"
     - `max_key` (string): 最大 key（最后一个 key），例如 "zebra"
     - `raw_key_size` (string, 可选): 所有 key 的总大小（字节）
     - `raw_value_size` (string, 可选): 所有 value 的总大小（字节）
   
   - **Statistics Meta Block 编码示例**:
     ```
     统计信息:
       num_keys = 1000000
       data_size = 10485760
       index_size = 4096
       filter_size = 1198
       min_key = "apple"
       max_key = "zebra"
     
     编码 (简化示例):
       Entry 0: "num_keys" → "1000000"
       Entry 1: "data_size" → "10485760"  (shared_len=0, 因为与 "num_keys" 无共享)
       Entry 2: "index_size" → "4096"     (shared_len=0)
       Entry 3: "filter_size" → "1198"     (shared_len=5, 共享 "size")
       Entry 4: "min_key" → "apple"        (shared_len=0)
       Entry 5: "max_key" → "zebra"        (shared_len=3, 共享 "_key")
     ```
   
   - **Statistics Meta Block 使用场景**:
     - **查询优化**: 使用 min_key 和 max_key 快速判断 key 是否在 SSTable 范围内
     - **Compaction 决策**: 使用 num_keys 和 data_size 决定是否需要 Compaction
     - **监控和调试**: 提供 SSTable 的详细统计信息
     - **成本估算**: 使用 raw_key_size 和 raw_value_size 估算存储成本

**Meta Block 查找流程**:

```
1. 从 Footer 读取 Meta Block Handle
2. 读取 Meta Block
3. 根据 Meta Block 类型解析内容:
   - Bloom Filter: 检查 key 是否可能存在
   - Statistics: 读取统计信息（用于优化）
```

**Bloom Filter 使用示例**:

```
查找 key="target" 的优化流程:

1. 读取 Meta Block (Bloom Filter)
2. 检查 Bloom Filter:
   if (!bloom_filter->MayContain("target")) {
       return NotFound;  // 快速返回，无需读取 Data Block
   }
3. 如果 Bloom Filter 返回 true，继续正常的查找流程:
   - 读取 Index Block
   - 查找对应的 Data Block
   - 在 Data Block 中查找
```

**性能优势**:
- **快速否定**: Bloom Filter 可以快速判断 key 不存在，避免 99% 的不必要 I/O
- **空间效率**: Bloom Filter 通常只占用每个 key 10 bits，空间开销小
- **I/O 减少**: 对于不存在的 key，只需读取 Meta Block（通常 < 1KB），而不是整个 Data Block（通常 4-64KB）

**Meta Block 存储结构**:

SSTable 文件可以包含多个 Meta Block，通过 Meta Index Block 管理：

```
Meta Index Block (格式与 Index Block 相同):
┌─────────────────────────────────────────────────────────┐
│ Entry 0: key="filter.bloom", BlockHandle{offset, size} │
│ Entry 1: key="stats", BlockHandle{offset, size}        │
│ ...                                                     │
└─────────────────────────────────────────────────────────┘
```

**Meta Block 查找流程**:

```
1. 从 Footer 读取 Meta Index Block Handle
2. 读取 Meta Index Block
3. 在 Meta Index Block 中查找目标 Meta Block 名称
4. 读取对应的 Meta Block
5. 根据 Meta Block 类型解析内容
```

**Meta Block 命名约定**:

```
Meta Block 通过名称标识:
- "filter.bloom": Bloom Filter（最常用）
- "filter.ribbon": Ribbon Filter（可选，更高效的替代方案）
- "stats": 统计信息
- "compression": 压缩信息（未来扩展）
- "encryption": 加密信息（未来扩展）
- "custom.*": 自定义 Meta Block（用户定义）
```

**Meta Block 完整查找示例**:

```
查找 key="target" 的完整流程:

1. 读取 Footer
   └─► 获取 Meta Index Block Handle

2. 读取 Meta Index Block
   └─► 查找 "filter.bloom" Entry
   └─► 获取 Bloom Filter BlockHandle

3. 读取 Bloom Filter Meta Block
   └─► 检查 "target" 是否可能存在
   └─► 如果不存在，直接返回 NotFound

4. 如果可能存在，继续查找:
   └─► 读取 Index Block
   └─► 查找对应的 Data Block
   └─► 在 Data Block 中查找 "target"
```

**Meta Block 性能影响**:

```
无 Bloom Filter:
  查找不存在的 key: 需要读取 Index Block + Data Block = ~8KB I/O

有 Bloom Filter:
  查找不存在的 key: 只需读取 Meta Index Block + Bloom Filter = ~2KB I/O
  性能提升: 约 4 倍（对于不存在的 key）
  
  查找存在的 key: 额外读取 Bloom Filter = ~1KB I/O
  性能影响: 可忽略（Bloom Filter 很小，通常 < 1KB）
```

#### Footer 格式

```
Footer (固定 48 字节):
┌─────────────────────────────────────────────────────────┐
│ Index Block Handle (offset + size, 16 bytes)            │
│  ┌──────────────┬────────────────────────────────────┐ │
│  │ Offset       │ Size                               │ │
│  │ (8 bytes)    │ (8 bytes)                          │ │
│  └──────────────┴────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────┤
│ Meta Block Handle (offset + size, 16 bytes)              │
│  ┌──────────────┬────────────────────────────────────┐ │
│  │ Offset       │ Size                               │ │
│  │ (8 bytes)    │ (8 bytes)                          │ │
│  └──────────────┴────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────┤
│ Padding (8 bytes)                                        │
│  (用于对齐，内容未定义)                                   │
├─────────────────────────────────────────────────────────┤
│ Magic Number (8 bytes) = 0xdb4775248b80fb57             │
│  (用于验证文件格式)                                       │
└─────────────────────────────────────────────────────────┘
```

**Footer 字段说明**:

1. **Index Block Handle**:
   - **Offset**: Index Block 在文件中的字节偏移量（从文件开头计算）
   - **Size**: Index Block 的字节大小
   - **用途**: 定位 Index Block，用于查找 Data Block

2. **Meta Block Handle**:
   - **Offset**: Meta Block 在文件中的字节偏移量
   - **Size**: Meta Block 的字节大小
   - **用途**: 定位 Meta Block（Bloom Filter、统计信息等）
   - **注意**: 如果 Meta Block 不存在，Size = 0

3. **Padding**:
   - **大小**: 8 字节
   - **用途**: 对齐到 8 字节边界，提高读取效率
   - **内容**: 未定义，通常为 0

4. **Magic Number**:
   - **值**: `0xdb4775248b80fb57` (固定值)
   - **用途**: 
     - 验证文件格式是否正确
     - 检测文件损坏
     - 区分不同版本的文件格式

**Footer 读取流程**:

```
1. 从文件末尾读取最后 48 字节
2. 验证 Magic Number 是否正确
3. 如果正确，解析 Index Block 和 Meta Block 的位置
4. 如果错误，返回文件格式错误
```

**设计优势**:
- **固定大小**: 48 字节固定大小，便于快速读取
- **文件末尾**: 放在文件末尾，写入时最后写入，保证原子性
- **自验证**: Magic Number 提供格式验证

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

> **注意**: 读性能 Scale Out 和一致性视图的详细设计已拆分为独立文档，请参考 [读性能 Scale Out 和一致性视图设计文档](READ_SCALE_OUT.md)。

该文档包含以下内容：
- 多版本并发控制 (MVCC) 设计
- 版本引用计数管理
- 读取路径优化策略
- 快照隔离 (Snapshot Isolation) 实现
- 序列号管理和版本可见性判断
- 快照生命周期管理
- 一致性保证级别
- 读性能 Scale Out 架构
- 一致性视图的使用场景

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
