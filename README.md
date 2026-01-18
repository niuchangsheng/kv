# LevelDB风格KV存储引擎

这是一个用 C++ 实现的轻量级键值（Key-Value）存储引擎，采用LevelDB的接口设计风格。提供简单、可靠的内存KV存储功能，支持批量操作和数据迭代。

## 特性

- **LevelDB兼容接口**：采用LevelDB的标准API设计
- **Status错误处理**：使用Status对象进行错误处理和状态反馈
- **批量操作支持**：WriteBatch支持原子性批量写入
- **数据迭代**：Iterator接口支持有序数据遍历
- **配置选项**：丰富的Options类用于控制读写行为
- **C++17标准**：使用现代C++特性开发

## 项目结构

```
├── CMakeLists.txt          # 主构建文件
├── README.md              # 项目说明
├── docs/                  # 项目文档
│   └── README.md          # 详细设计文档
├── src/                   # 源代码
│   ├── CMakeLists.txt     # 源码构建配置
│   ├── kv_engine.h        # 主接口头文件
│   ├── kv_engine.cpp      # 主接口实现
│   ├── status.h/cpp       # 状态管理
│   ├── options.h/cpp      # 配置选项
│   ├── iterator.h/cpp     # 迭代器接口
│   ├── write_batch.h/cpp  # 批量操作
│   ├── db_iterator.h/cpp  # 迭代器实现
│   └── main.cpp           # 测试程序
└── test/                  # 单元测试
```

## 构建（CMake）

### Linux / macOS

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

### Windows (Visual Studio)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## 快速使用示例

```cpp
#include "kv_engine.h"

int main() {
    // 打开数据库
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb", &db);

    if (!status.ok()) {
        std::cerr << "无法打开数据库: " << status.ToString() << std::endl;
        return 1;
    }

    // 写入数据
    WriteOptions write_options;
    status = db->Put(write_options, "name", "Alice");
    status = db->Put(write_options, "age", "25");

    // 读取数据
    ReadOptions read_options;
    std::string value;
    status = db->Get(read_options, "name", &value);
    if (status.ok()) {
        std::cout << "name: " << value << std::endl;
    }

    // 批量操作
    WriteBatch batch;
    batch.Put("city", "Beijing");
    batch.Delete("age");
    status = db->Write(write_options, &batch);

    // 数据迭代
    Iterator* it = db->NewIterator(read_options);
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::cout << it->key() << ": " << it->value() << std::endl;
    }
    delete it;

    delete db;
    return 0;
}
```

## API接口

### 核心类

- **DB**: 主数据库类，提供基本的KV操作
- **Status**: 操作状态和错误信息
- **Options/ReadOptions/WriteOptions**: 配置选项
- **WriteBatch**: 批量操作
- **Iterator**: 数据迭代器

### Status类

Status类用于表示操作的成功或失败状态，采用LevelDB的设计风格。

#### 状态类型

```cpp
// 成功状态
Status::OK()

// 错误状态
Status::NotFound("Not Found")          // 键不存在
Status::Corruption("Corruption")       // 数据损坏
Status::NotSupported("Not Supported")  // 操作不支持
Status::InvalidArgument("Invalid Argument")  // 参数错误
Status::IOError("IO Error")            // IO错误
```

#### 使用方法

```cpp
Status status = db->Get(read_options, "key", &value);
if (status.ok()) {
    // 操作成功
    std::cout << "Value: " << value << std::endl;
} else if (status.IsNotFound()) {
    // 键不存在
    std::cout << "Key not found" << std::endl;
} else {
    // 其他错误
    std::cerr << "Error: " << status.ToString() << std::endl;
}
```

### 配置选项类

#### Options类

数据库打开时的配置选项：

```cpp
Options options;
options.create_if_missing = true;      // 如果数据库不存在则创建
options.error_if_exists = false;       // 如果数据库已存在则报错
options.paranoid_checks = false;       // 是否进行严格的数据校验
// options.info_log = nullptr;         // 日志输出（可选）
```

#### ReadOptions类

读取操作的配置选项：

```cpp
ReadOptions read_options;
read_options.verify_checksums = false;  // 是否验证校验和
read_options.fill_cache = true;         // 是否使用缓存
// read_options.snapshot = nullptr;     // 快照读取（可选）
```

#### WriteOptions类

写入操作的配置选项：

```cpp
WriteOptions write_options;
write_options.sync = false;             // 是否同步写入磁盘
```

### 主要方法

```cpp
// 数据库操作
static Status Open(const Options& options, const std::string& name, DB** dbptr);
Status Put(const WriteOptions& options, const std::string& key, const std::string& value);
Status Get(const ReadOptions& options, const std::string& key, std::string* value);
Status Delete(const WriteOptions& options, const std::string& key);
Status Write(const WriteOptions& options, WriteBatch* updates);

// 迭代器操作
Iterator* NewIterator(const ReadOptions& options);
```

## 设计架构

- **存储引擎**: 当前使用内存unordered_map，可扩展为持久化存储
- **错误处理**: 统一的Status类，支持多种错误类型
- **批量操作**: WriteBatch确保操作的原子性
- **数据遍历**: Iterator提供有序的数据访问接口
- **配置管理**: 多层次的配置选项控制行为

## 测试

运行测试程序：

```bash
cd build
./kv_engine_app
```

## 后续规划

- [ ] 持久化存储支持（WAL + SSTable）
- [ ] 压缩与合并 (compaction)
- [ ] 并发控制优化
- [ ] 范围查询支持
- [ ] 内存缓存层
- [ ] 网络协议支持

## 贡献

欢迎提交 issue 和 PR。请包含：
- 问题描述
- 重现步骤
- 预期行为和实际行为

## 许可证

请在仓库中添加许可证文件。
