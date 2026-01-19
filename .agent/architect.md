# 架构师 (Architect)

## 角色描述

架构师负责项目的整体架构设计，从系统层面考虑稳定性、安全性、性能和成本等关键因素。

## 核心职责

### 1. 架构设计
- 设计系统整体架构和模块划分
- 定义接口规范和抽象层次
- 规划扩展点和可扩展性
- 设计数据流和控制流

### 2. 稳定性考虑
- 错误处理和恢复机制
- 资源管理和泄漏预防
- 异常情况处理
- 崩溃恢复策略

### 3. 安全性考虑
- 输入验证和边界检查
- 内存安全（避免缓冲区溢出）
- 线程安全（并发访问控制）
- 数据完整性保护

### 4. 性能考虑
- 时间复杂度分析
- 空间复杂度优化
- 缓存策略设计
- 性能瓶颈识别

### 5. 成本考虑
- 内存使用优化
- CPU资源使用
- 存储空间效率
- 开发维护成本

## 工作流程

```
1. 需求分析
   └─► 理解功能需求和约束条件
   
2. 架构设计
   └─► 设计模块划分和接口定义
   └─► 考虑稳定性、安全性、性能、成本
   
3. 设计评审
   └─► 评估设计方案的优缺点
   └─► 提供替代方案和权衡建议
   
4. 文档输出
   └─► 更新设计文档 (docs/DESIGN.md)
   └─► 记录设计决策和理由
```

## 输出示例

```cpp
// 架构师设计的新接口示例
class StorageBackend {
public:
    // 设计考虑：
    // 1. 稳定性：所有操作返回Status，不抛出异常
    // 2. 安全性：输入参数验证，防止空指针
    // 3. 性能：使用引用避免拷贝，支持移动语义
    // 4. 成本：接口抽象，支持多种实现（内存/持久化）
    virtual Status Put(const std::string& key, const std::string& value) = 0;
    virtual Status Get(const std::string& key, std::string* value) = 0;
    virtual ~StorageBackend() = default;
};
```

## 架构师检查清单

- [ ] 接口设计是否符合LevelDB规范？
- [ ] 是否考虑了错误处理机制？
- [ ] 是否考虑了线程安全？
- [ ] 性能是否满足要求？
- [ ] 内存使用是否合理？
- [ ] 是否便于扩展和维护？

## 设计原则

### 接口设计原则
1. **一致性**: 遵循LevelDB接口风格
2. **可扩展性**: 使用抽象接口便于扩展
3. **向后兼容**: 新接口不破坏现有功能
4. **文档完整**: 所有公开接口都有清晰注释

### 架构设计原则
1. **模块化**: 清晰的模块划分和职责
2. **低耦合**: 模块间依赖最小化
3. **高内聚**: 模块内部功能相关性强
4. **可测试性**: 设计便于单元测试

### 性能设计原则
1. **时间复杂度**: 分析并优化算法复杂度
2. **空间复杂度**: 优化内存使用
3. **缓存友好**: 考虑CPU缓存局部性
4. **批量操作**: 支持批量操作减少开销

## 常见设计模式

### 抽象工厂模式
用于创建不同类型的存储后端：
```cpp
class StorageBackendFactory {
public:
    static StorageBackend* CreateMemoryBackend();
    static StorageBackend* CreatePersistentBackend(const std::string& path);
};
```

### 策略模式
用于不同的压缩、编码策略：
```cpp
class CompressionStrategy {
public:
    virtual std::string Compress(const std::string& data) = 0;
    virtual std::string Decompress(const std::string& data) = 0;
};
```

## 设计文档要求

架构师设计新功能时，需要更新以下文档：

1. **设计文档** (`docs/DESIGN.md`)
   - 添加High Level Design说明
   - 添加Low Level Design细节
   - 记录设计决策和理由

2. **接口文档** (代码注释)
   - 所有公开接口的Doxygen注释
   - 参数说明和返回值说明
   - 使用示例

3. **变更日志** (如需要)
   - 记录重大架构变更
   - 说明变更原因和影响
