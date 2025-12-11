# KV 存储系统

这是一个用 C++ 实现的轻量级键值（Key-Value）存储系统，使用 CMake 构建。目标是提供简单、可靠且高性能的本地持久化 KV 存储，适合作为学习项目或嵌入式/服务端组件的基础。

## 特性

- 简单的 C++ API：`put/get/delete` 等基本操作
- 持久化到磁盘（可扩展为 WAL + 数据文件）
- 内存索引以加速读取
- 基本并发读写支持（可扩展为更复杂的并发控制）

## 项目结构（示例）

- `CMakeLists.txt` - 构建入口
- `src/` - 源代码
- `include/` - 头文件
- `tests/` - 单元测试
- `examples/` - 使用示例

（实际结构请以仓库内容为准）

## 构建（CMake）

在 Linux / WSL 或类 Unix 系统：

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

在 Windows（使用 Visual Studio）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

如果项目提供 `install` 规则，可运行：

```bash
cmake --install build --prefix ./install
```

## 快速使用示例（伪代码）

以下为示例 API 用法，实际函数名/命名空间请参照代码实现：

```cpp
#include "kv_store.h"

int main() {
    KVStore store("./data");
    store.put("user:1", "{\"name\":\"Alice\"}");
    auto val = store.get("user:1");
    if (val) std::cout << *val << std::endl;
    store.del("user:1");
    store.close();
    return 0;
}
```

## 设计要点（建议）

- 存储格式：可采用简单的日志结构（WAL）+ 数据文件，周期性合并(compaction)
- 索引：将键映射到文件偏移量的内存哈希表，加快读取
- 原子性：写入先写 WAL，确保崩溃恢复
- 并发：读多写少场景下使用读写锁；需要更高吞吐可引入 sharding 或 lock-free 结构

## 测试

建议添加单元测试覆盖基本功能（put/get/delete、持久化恢复、并发读写）。可用 `CTest` 或 GoogleTest。

## 后续规划

- 支持范围查询/前缀扫描
- 压缩与合并 (compaction)
- 可选内存缓存层
- 网络协议（简单 RPC）以支持远程访问

## 贡献

欢迎提交 issue 和 PR。请包含：问题描述、重现步骤、预期行为和实际行为。

## 许可证

请在仓库中添加许可证文件（例如 `LICENSE`），并在此处注明许可证类型。
