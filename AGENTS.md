# AI Agent Configuration Guide

本文档为AI代理（如GitHub Copilot、Cursor AI、Claude等）提供项目上下文和编码指导，帮助AI更好地理解项目结构、设计原则和编码规范。

## 项目概述

这是一个**LevelDB风格的键值存储引擎**，使用C++17实现。项目遵循LevelDB的接口设计，提供简单、可靠的KV存储功能。

### 核心设计原则

1. **LevelDB兼容性**: 所有公开接口必须遵循LevelDB的标准API设计
2. **Status错误处理**: 使用Status对象而非异常处理错误
3. **RAII资源管理**: 所有资源使用RAII原则自动管理
4. **值语义**: Status等轻量级对象使用值语义，可复制
5. **接口抽象**: 使用抽象接口便于扩展（如Iterator）

## 代码风格和规范

### 命名规范

#### 类名
- **PascalCase**: `DB`, `Status`, `WriteBatch`, `DBIterator`
- **示例**: 
  ```cpp
  class DB { ... };
  class WriteBatch { ... };
  ```

#### 方法名
- **PascalCase**: 所有公开方法使用PascalCase
- **示例**: 
  ```cpp
  Status Put(const WriteOptions& options, const std::string& key, ...);
  Status Get(const ReadOptions& options, const std::string& key, ...);
  Iterator* NewIterator(const ReadOptions& options);
  ```

#### 成员变量
- **snake_case + 下划线后缀**: 私有成员变量使用下划线后缀
- **示例**:
  ```cpp
  class DB {
  private:
      std::unordered_map<std::string, std::string> data_store_;
  };
  ```

#### 局部变量
- **snake_case**: 普通局部变量
- **示例**:
  ```cpp
  std::string value;
  Status status;
  ```

### 代码格式

#### 头文件保护
```cpp
#ifndef DB_H
#define DB_H
// ... code ...
#endif // DB_H
```

#### 包含顺序
1. 对应的头文件（如果有）
2. 项目内部头文件
3. 标准库头文件
4. 第三方库头文件

```cpp
#include "db.h"        // 对应头文件
#include "status.h"            // 项目内部
#include <string>              // 标准库
#include <unordered_map>       // 标准库
```

#### 注释风格
- **类和方法**: 使用Doxygen风格注释
- **示例**:
  ```cpp
  // Open the database with the specified "name".
  // Stores a pointer to a heap-allocated database in *dbptr and returns
  // OK on success.
  static Status Open(const Options& options, const std::string& name, DB** dbptr);
  ```

### 错误处理模式

**必须使用Status对象，不要抛出异常**

```cpp
// ✅ 正确
Status status = db->Get(read_options, key, &value);
if (!status.ok()) {
    return status;
}

// ❌ 错误 - 不要这样做
if (error) {
    throw std::runtime_error("Error occurred");
}
```

### 资源管理

**使用RAII，避免手动内存管理**

```cpp
// ✅ 正确 - 使用智能指针或RAII
class DB {
    // 自动管理资源
};

// ✅ 正确 - Iterator需要手动delete（遵循LevelDB接口）
Iterator* it = db->NewIterator(read_options);
// ... use it ...
delete it;  // 用户负责释放

// ❌ 错误 - 不要使用裸指针管理资源
DB* db = new DB();  // 应该使用DB::Open
```

## 项目结构

### 目录组织

```
kv/
├── src/                    # 源代码
│   ├── db.h/cpp     # 主DB接口
│   ├── status.h/cpp        # 状态管理
│   ├── options.h/cpp       # 配置选项
│   ├── iterator.h/cpp      # 迭代器接口
│   ├── write_batch.h/cpp   # 批量操作
│   └── db_iterator.h/cpp  # 迭代器实现
├── test/                   # 单元测试
│   └── test_db.cpp
├── docs/                   # 文档
│   └── ARCHITECTURE.md          # 架构设计文档
└── build/bin/             # 构建输出
```

### 文件命名

- **头文件**: `snake_case.h` (如 `db.h`, `write_batch.h`)
- **源文件**: `snake_case.cpp` (如 `db.cpp`, `write_batch.cpp`)
- **测试文件**: `test_*.cpp` (如 `test_db.cpp`)

## 核心组件

### 1. DB类 (db.h/cpp)

**职责**: 主数据库接口，提供CRUD操作

**关键点**:
- 使用静态方法`Open()`创建实例（Factory模式）
- 所有操作返回`Status`对象
- 使用`Options`/`ReadOptions`/`WriteOptions`配置操作
- 提供`NewIterator()`创建迭代器

**接口模式**:
```cpp
class DB {
public:
    static Status Open(const Options& options, const std::string& name, DB** dbptr);
    Status Put(const WriteOptions& options, const std::string& key, const std::string& value);
    Status Get(const ReadOptions& options, const std::string& key, std::string* value);
    Status Delete(const WriteOptions& options, const std::string& key);
    Status Write(const WriteOptions& options, WriteBatch* updates);
    Iterator* NewIterator(const ReadOptions& options);
};
```

### 2. Status类 (status.h/cpp)

**职责**: 统一的错误处理机制

**关键点**:
- 值语义，可复制
- 提供静态工厂方法（`OK()`, `NotFound()`, `Corruption()`等）
- 提供检查方法（`ok()`, `IsNotFound()`, `IsCorruption()`等）
- 提供`ToString()`获取错误信息

**使用模式**:
```cpp
Status status = db->Get(read_options, key, &value);
if (status.ok()) {
    // 成功处理
} else if (status.IsNotFound()) {
    // 键不存在
} else {
    // 其他错误
    std::cerr << status.ToString() << std::endl;
}
```

### 3. Options类族 (options.h/cpp)

**职责**: 配置管理

**关键点**:
- `Options`: 数据库级别配置
- `ReadOptions`: 读取操作配置
- `WriteOptions`: 写入操作配置
- 使用结构体而非类（简单配置）

### 4. Iterator接口 (iterator.h/cpp)

**职责**: 数据遍历抽象接口

**关键点**:
- 抽象基类，纯虚函数
- 提供`Seek()`, `Next()`, `Prev()`, `key()`, `value()`等方法
- 提供`status()`方法返回错误状态
- 用户负责`delete`迭代器

### 5. WriteBatch类 (write_batch.h/cpp)

**职责**: 批量操作支持

**关键点**:
- 支持`Put()`和`Delete()`操作的批量添加
- 通过`DB::Write()`原子性执行
- 使用内部`Rep`结构隐藏实现细节

## 设计模式

### Factory模式
```cpp
// DB通过静态方法Open创建
DB* db;
Status status = DB::Open(options, name, &db);
```

### Iterator模式
```cpp
// 提供统一的遍历接口
Iterator* it = db->NewIterator(read_options);
for (it->SeekToFirst(); it->Valid(); it->Next()) {
    // 处理数据
}
```

### PIMPL模式（部分使用）
```cpp
// WriteBatch使用内部Rep结构
class WriteBatch {
private:
    struct Rep;
    Rep* rep_;
};
```

## 常见任务和模式

### 添加新的错误类型

1. 在`Status`类中添加新的错误代码枚举
2. 添加静态工厂方法
3. 添加检查方法（如`IsXXX()`）
4. 更新`ToString()`方法

```cpp
// status.h
enum Code {
    // ... existing codes ...
    kNewError = 6
};

static Status NewError(const std::string& msg = "New Error") {
    return Status(kNewError, msg);
}

bool IsNewError() const { return code_ == kNewError; }
```

### 添加新的配置选项

1. 在相应的Options类中添加成员变量
2. 在构造函数中初始化默认值
3. 在文档中说明用途

```cpp
// options.h
class ReadOptions {
public:
    bool new_option;  // 新选项
};

// options.cpp
ReadOptions::ReadOptions()
    : verify_checksums(false),
      fill_cache(true),
      new_option(false) {  // 默认值
}
```

### 实现新的Iterator

1. 继承`Iterator`抽象类
2. 实现所有纯虚函数
3. 在`DB::NewIterator()`中返回新实例

```cpp
class NewIterator : public Iterator {
public:
    virtual bool Valid() const override;
    virtual void Seek(const std::string& target) override;
    // ... 实现所有方法
};
```

## 反模式（避免）

### ❌ 不要使用异常

```cpp
// ❌ 错误
if (error) {
    throw std::runtime_error("Error");
}

// ✅ 正确
if (error) {
    return Status::Corruption("Error message");
}
```

### ❌ 不要使用bool返回值

```cpp
// ❌ 错误
bool Get(const std::string& key, std::string* value);

// ✅ 正确
Status Get(const ReadOptions& options, const std::string& key, std::string* value);
```

### ❌ 不要使用camelCase方法名

```cpp
// ❌ 错误
Status put(const std::string& key, const std::string& value);

// ✅ 正确
Status Put(const WriteOptions& options, const std::string& key, const std::string& value);
```

### ❌ 不要忽略Options参数

```cpp
// ❌ 错误
Status Put(const std::string& key, const std::string& value);

// ✅ 正确
Status Put(const WriteOptions& options, const std::string& key, const std::string& value);
```

### ❌ 不要使用智能指针管理Iterator

```cpp
// ❌ 错误 - 违反LevelDB接口设计
std::unique_ptr<Iterator> NewIterator(const ReadOptions& options);

// ✅ 正确 - 遵循LevelDB接口
Iterator* NewIterator(const ReadOptions& options);
```

## 测试要求

### 单元测试规范

1. **每个新功能必须包含测试**
2. **测试文件命名**: `test_*.cpp`
3. **测试函数命名**: `test_*()` 或使用Google Test的`TEST()`宏

```cpp
// test/test_db.cpp
void test_new_feature() {
    // 测试代码
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/test", &db);
    assert(status.ok());
    // ... 更多测试
    delete db;
}
```

### 测试覆盖要求

- ✅ 正常路径测试
- ✅ 错误路径测试（NotFound, Corruption等）
- ✅ 边界条件测试
- ✅ 批量操作测试

## CMake配置

### 添加新源文件

在`src/CMakeLists.txt`中添加：

```cmake
add_library(kv_lib
    db.cpp
    status.cpp
    # ... existing files ...
    new_file.cpp  # 新文件
)

target_sources(kv_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            db.h
            # ... existing headers ...
            new_file.h  # 新头文件
)
```

### 输出目录

所有二进制文件输出到`build/bin/`目录（已在主CMakeLists.txt中配置）。

## 提交规范

### Commit Message格式

```
<type>: <subject>

<body>

<footer>
```

**类型**:
- `feat`: 新功能
- `fix`: Bug修复
- `docs`: 文档更新
- `refactor`: 代码重构
- `test`: 测试相关
- `build`: 构建系统相关

**示例**:
```
feat: add persistent storage support

- Implement WAL (Write-Ahead Logging)
- Add SSTable file format
- Support database recovery

Closes #123
```

## 性能考虑

### 当前实现性能特征

- **Put/Get/Delete**: O(1) 平均时间复杂度
- **Iterator构造**: O(n log n) - 需要排序
- **Iterator Seek**: O(log n) - 二分查找
- **WriteBatch**: O(n) - n为操作数量

### 优化原则

1. **避免不必要的拷贝**: 使用引用和移动语义
2. **预分配**: 对于已知大小的容器，预分配容量
3. **字符串优化**: 考虑使用`string_view`（C++17）

## 扩展指南

### 添加持久化存储

1. 创建`StorageBackend`抽象接口
2. 实现`MemoryStorage`（当前实现）
3. 实现`PersistentStorage`（未来）
4. 在`DB`类中使用存储后端

### 添加并发支持

1. 添加读写锁保护`data_store_`
2. 在`Put/Get/Delete`操作中加锁
3. 考虑无锁数据结构（高级）

## 调试技巧

### 常见问题

1. **Status检查**: 总是检查`status.ok()`
2. **Iterator有效性**: 在访问`key()`/`value()`前检查`Valid()`
3. **资源清理**: 确保`delete`所有`Iterator`和`DB`对象

### 调试工具

- **GDB**: 调试C++程序
- **Valgrind**: 内存泄漏检查
- **AddressSanitizer**: 内存错误检测

## 参考文档

- **设计文档**: `docs/ARCHITECTURE.md` - 详细的High/Low Level设计
- **技能要求**: `SKILLS.md` - 开发所需技能
- **README**: `README.md` - 项目概述和使用指南

## Subagent角色定义

本项目定义了多个专门的AI代理角色，每个角色负责不同的职责。AI代理可以根据任务类型自动切换角色，或由用户明确指定角色。

**详细角色定义请参考**: [.agent/README.md](.agent/README.md)

### 角色概览

| 角色 | 主要职责 | 关注点 | 详细文档 |
|------|---------|--------|---------|
| **架构师** | 整体架构设计 | 稳定性、安全性、性能、成本 | [.agent/architect.md](.agent/architect.md) |
| **研发工程师** | 代码实现 | 功能实现、编译通过、代码质量 | [.agent/developer.md](.agent/developer.md) |
| **测试工程师** | 测试设计 | 测试用例、代码覆盖率、性能回归 | [.agent/test_engineer.md](.agent/test_engineer.md) |
| **代码审查员** | 代码审查 | 代码质量、规范符合、最佳实践 | [.agent/code_reviewer.md](.agent/code_reviewer.md) |

### 快速参考

- **架构师**: 负责系统架构设计，考虑稳定性、安全性、性能和成本
- **研发工程师**: 负责代码实现，确保编译通过和代码质量
- **测试工程师**: 负责测试设计和执行，确保代码覆盖率和性能
- **代码审查员**: 负责代码审查，确保代码质量和规范符合

详细的角色定义、工作流程、检查清单和代码示例请查看对应的文档文件。

---

## 角色协作流程

### 典型开发流程

```
1. 架构师
   └─► 设计接口和架构
   └─► 输出：设计文档、接口定义
   
2. 研发工程师
   └─► 实现代码
   └─► 输出：源代码、编译通过
   
3. 测试工程师
   └─► 编写测试用例
   └─► 输出：测试代码、覆盖率报告
   
4. 代码审查员
   └─► 审查代码和测试
   └─► 输出：审查反馈、改进建议
   
5. 研发工程师/测试工程师
   └─► 根据反馈修改代码
   └─► 重新提交审查
   
6. 代码审查员（最终）
   └─► 确认问题已修复
   └─► 批准合并
```

### 角色切换提示

AI代理可以根据任务类型自动切换角色：

- **设计新功能** → 架构师角色
- **实现代码** → 研发工程师角色
- **编写测试** → 测试工程师角色
- **代码审查** → 代码审查员角色
- **架构评审** → 架构师角色

### 角色指定方式

在提示中明确指定角色：

```
作为架构师，请设计一个持久化存储接口...
作为研发工程师，请实现DB::Put方法...
作为测试工程师，请为WriteBatch编写测试用例...
作为代码审查员，请审查这个PR的代码质量...
```

## AI代理提示

当为这个项目生成代码时，请：

1. ✅ **遵循LevelDB接口风格** - 所有公开接口必须与LevelDB兼容
2. ✅ **使用Status错误处理** - 不要使用异常
3. ✅ **使用PascalCase方法名** - 所有公开方法
4. ✅ **使用snake_case成员变量** - 带下划线后缀
5. ✅ **添加适当的注释** - 特别是公开接口
6. ✅ **编写单元测试** - 为新功能添加测试
7. ✅ **更新CMakeLists.txt** - 添加新文件到构建系统
8. ✅ **遵循RAII原则** - 自动资源管理
9. ✅ **保持代码简洁** - 避免过度设计
10. ✅ **参考现有代码** - 保持风格一致

---

**最后更新**: 请定期查看本文档，确保与项目最新状态同步。
