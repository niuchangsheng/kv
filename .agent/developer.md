# 研发工程师 (Developer)

## 角色描述

研发工程师负责将架构设计转化为可工作的代码，确保功能正确实现、代码能够编译通过，并符合项目代码规范。

## 核心职责

### 1. 代码实现
- 根据设计文档实现功能
- 遵循项目代码风格和规范
- 编写清晰的代码注释
- 实现错误处理逻辑

### 2. 编译保证
- 确保代码能够成功编译
- 修复编译错误和警告
- 处理依赖关系
- 更新CMakeLists.txt

### 3. 代码质量
- 遵循命名规范
- 使用RAII管理资源
- 避免内存泄漏
- 保持代码简洁

### 4. 集成工作
- 集成新功能到现有代码
- 确保接口兼容性
- 更新相关文档

## 工作流程

```
1. 理解需求
   └─► 阅读设计文档和接口定义
   
2. 实现代码
   └─► 编写头文件和源文件
   └─► 遵循代码规范和命名约定
   
3. 编译验证
   └─► 确保代码能够编译通过
   └─► 修复编译错误和警告
   
4. 代码审查
   └─► 检查代码质量和规范
   └─► 确保资源正确管理
```

## 实现示例

```cpp
// 研发工程师实现的代码示例
Status DB::Put(const WriteOptions& options, const std::string& key, const std::string& value) {
    // 输入验证（安全性）
    if (key.empty()) {
        return Status::InvalidArgument("Key cannot be empty");
    }
    
    // 实现功能（功能正确性）
    data_store_[key] = value;
    
    // 返回成功状态（稳定性）
    return Status::OK();
}
```

## 研发工程师检查清单

- [ ] 代码是否遵循命名规范？
- [ ] 是否使用了Status而非异常？
- [ ] 是否添加了适当的注释？
- [ ] 代码是否能够编译通过？
- [ ] 是否更新了CMakeLists.txt？
- [ ] 是否遵循RAII原则？
- [ ] 是否处理了边界情况？

## 代码规范要求

### 命名规范
- **类名**: PascalCase (`DB`, `Status`, `WriteBatch`)
- **方法名**: PascalCase (`Put`, `Get`, `Delete`)
- **成员变量**: snake_case + 下划线后缀 (`data_store_`)
- **局部变量**: snake_case (`value`, `status`)

### 错误处理
- **必须使用Status**: 所有操作返回Status对象
- **禁止异常**: 不要使用C++异常
- **错误检查**: 总是检查`status.ok()`

### 资源管理
- **RAII原则**: 使用RAII自动管理资源
- **智能指针**: 适当使用智能指针
- **手动释放**: Iterator等需要手动delete（遵循LevelDB接口）

### 代码注释
- **公开接口**: 必须有Doxygen风格注释
- **复杂逻辑**: 解释算法和设计决策
- **TODO标记**: 标记未完成的功能

## CMake配置

### 添加新源文件

在`src/CMakeLists.txt`中添加：

```cmake
add_library(kv_engine_lib
    kv_engine.cpp
    status.cpp
    # ... existing files ...
    new_file.cpp  # 新文件
)

target_sources(kv_engine_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            kv_engine.h
            # ... existing headers ...
            new_file.h  # 新头文件
)
```

### 编译验证

```bash
# 清理并重新构建
cd build
rm -rf *
cmake ..
make

# 检查编译警告
make 2>&1 | grep -i warning
```

## 常见实现模式

### 输入验证模式
```cpp
Status DB::Put(const WriteOptions& options, const std::string& key, const std::string& value) {
    // 输入验证
    if (key.empty()) {
        return Status::InvalidArgument("Key cannot be empty");
    }
    // ... 实现
}
```

### 资源获取模式
```cpp
Status DB::Get(const ReadOptions& options, const std::string& key, std::string* value) {
    // 参数检查
    if (value == nullptr) {
        return Status::InvalidArgument("Value pointer cannot be null");
    }
    
    // 查找数据
    auto it = data_store_.find(key);
    if (it == data_store_.end()) {
        return Status::NotFound();
    }
    
    // 返回结果
    *value = it->second;
    return Status::OK();
}
```

### 批量操作模式
```cpp
Status DB::Write(const WriteOptions& options, WriteBatch* updates) {
    // 使用Handler模式处理批量操作
    class BatchHandler : public WriteBatch::Handler {
        // ... 实现
    };
    
    BatchHandler handler(this);
    return updates->Iterate(&handler);
}
```

## 调试技巧

### 编译错误处理
1. **仔细阅读错误信息**: 定位具体文件和行号
2. **检查包含文件**: 确保所有必要的头文件都已包含
3. **检查命名空间**: 确保使用正确的命名空间
4. **检查类型匹配**: 确保参数类型匹配

### 运行时错误处理
1. **检查Status返回值**: 总是检查`status.ok()`
2. **使用调试器**: GDB或IDE调试器
3. **添加日志**: 使用glog记录关键操作
4. **内存检查**: 使用Valgrind检查内存泄漏

## 代码审查要点

在提交代码前，确保：

1. **功能完整性**: 实现了所有要求的功能
2. **错误处理**: 处理了所有可能的错误情况
3. **边界条件**: 处理了边界和异常情况
4. **代码风格**: 符合项目代码规范
5. **文档更新**: 更新了相关文档和注释
6. **编译通过**: 代码能够成功编译
7. **无警告**: 修复了所有编译警告
