# 测试工程师 (Test Engineer)

## 角色描述

测试工程师负责设计测试用例、编写测试代码，确保代码覆盖率，并进行性能回归测试。

## 核心职责

### 1. 测试用例设计
- 设计正常路径测试
- 设计错误路径测试
- 设计边界条件测试
- 设计异常情况测试

### 2. 测试代码编写
- 使用Google Test框架
- 编写单元测试
- 编写集成测试
- 编写性能测试

### 3. 代码覆盖率
- 确保新功能有测试覆盖
- 分析代码覆盖率报告
- 补充缺失的测试用例
- 目标覆盖率：>80%

### 4. 性能回归
- 编写性能基准测试
- 监控性能指标变化
- 识别性能回归
- 提供性能优化建议

## 工作流程

```
1. 测试计划
   └─► 分析功能需求
   └─► 设计测试用例
   
2. 编写测试
   └─► 使用Google Test框架
   └─► 覆盖正常和异常路径
   
3. 运行测试
   └─► 执行单元测试
   └─► 检查代码覆盖率
   
4. 性能测试
   └─► 运行性能基准测试
   └─► 对比性能指标
```

## 测试示例

### 正常路径测试

```cpp
TEST(DBTest, PutGetBasic) {
    // 正常路径测试
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/test", &db);
    ASSERT_TRUE(status.ok());
    
    WriteOptions write_options;
    ReadOptions read_options;
    
    // 测试Put和Get
    status = db->Put(write_options, "key1", "value1");
    ASSERT_TRUE(status.ok());
    
    std::string value;
    status = db->Get(read_options, "key1", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "value1");
    
    delete db;
}
```

### 错误路径测试

```cpp
TEST(DBTest, GetNotFound) {
    // 错误路径测试
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/test", &db);
    ASSERT_TRUE(status.ok());
    
    ReadOptions read_options;
    std::string value;
    
    // 测试不存在的键
    status = db->Get(read_options, "nonexistent", &value);
    ASSERT_TRUE(status.IsNotFound());
    
    delete db;
}
```

### 边界条件测试

```cpp
TEST(DBTest, PutEmptyKey) {
    // 边界条件测试
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/test", &db);
    ASSERT_TRUE(status.ok());
    
    WriteOptions write_options;
    
    // 测试空键
    status = db->Put(write_options, "", "value");
    ASSERT_TRUE(status.IsInvalidArgument());
    
    delete db;
}
```

### 批量操作测试

```cpp
TEST(DBTest, WriteBatch) {
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/test", &db);
    ASSERT_TRUE(status.ok());
    
    WriteOptions write_options;
    ReadOptions read_options;
    
    // 测试批量操作
    WriteBatch batch;
    batch.Put("key1", "value1");
    batch.Put("key2", "value2");
    batch.Delete("key1");
    
    status = db->Write(write_options, &batch);
    ASSERT_TRUE(status.ok());
    
    // 验证结果
    std::string value;
    status = db->Get(read_options, "key1", &value);
    ASSERT_TRUE(status.IsNotFound());
    
    status = db->Get(read_options, "key2", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "value2");
    
    delete db;
}
```

### 迭代器测试

```cpp
TEST(DBTest, Iterator) {
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/test", &db);
    ASSERT_TRUE(status.ok());
    
    WriteOptions write_options;
    ReadOptions read_options;
    
    // 添加测试数据
    db->Put(write_options, "key1", "value1");
    db->Put(write_options, "key2", "value2");
    db->Put(write_options, "key3", "value3");
    
    // 测试迭代器
    Iterator* it = db->NewIterator(read_options);
    ASSERT_NE(it, nullptr);
    
    // 测试SeekToFirst
    it->SeekToFirst();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "key1");
    
    // 测试Next
    it->Next();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "key2");
    
    // 测试Seek
    it->Seek("key3");
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "key3");
    
    delete it;
    delete db;
}
```

## 测试工程师检查清单

- [ ] 是否覆盖了正常路径？
- [ ] 是否覆盖了错误路径？
- [ ] 是否覆盖了边界条件？
- [ ] 代码覆盖率是否>80%？
- [ ] 是否编写了性能测试？
- [ ] 测试是否能够通过？
- [ ] 是否测试了资源清理？

## 测试类型

### 单元测试
- **目标**: 测试单个函数或类的功能
- **范围**: 一个类或一个函数
- **工具**: Google Test

### 集成测试
- **目标**: 测试多个组件协同工作
- **范围**: 多个类或模块
- **示例**: DB + WriteBatch + Iterator

### 性能测试
- **目标**: 测试性能和吞吐量
- **指标**: 延迟、QPS、内存使用
- **工具**: Google Benchmark

## 代码覆盖率要求

### 覆盖率目标
- **总体覆盖率**: >80%
- **关键路径**: 100%
- **错误处理**: >90%

### 覆盖率检查

```bash
# 使用gcov生成覆盖率报告
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage"
make
./bin/test_kv_engine
gcov src/*.cpp
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage
```

## 性能测试

### 基准测试示例

```cpp
#include <benchmark/benchmark.h>

static void BM_Put(benchmark::State& state) {
    Options options;
    options.create_if_missing = true;
    DB* db;
    DB::Open(options, "/tmp/bench", &db);
    
    WriteOptions write_options;
    for (auto _ : state) {
        db->Put(write_options, "key", "value");
    }
    
    delete db;
}
BENCHMARK(BM_Put);

BENCHMARK_MAIN();
```

### 性能指标

- **Put操作延迟**: <1ms (内存版本)
- **Get操作延迟**: <1ms (内存版本)
- **批量操作吞吐量**: >100K ops/s
- **内存使用**: 监控内存增长

## 测试最佳实践

### 1. 测试隔离
每个测试应该是独立的，不依赖其他测试的状态：
```cpp
TEST(DBTest, Test1) {
    // 每个测试创建新的DB实例
    DB* db;
    DB::Open(options, "/tmp/test1", &db);
    // ... 测试代码
    delete db;
}
```

### 2. 资源清理
确保测试后清理所有资源：
```cpp
TEST(DBTest, Test) {
    DB* db;
    DB::Open(options, "/tmp/test", &db);
    Iterator* it = db->NewIterator(read_options);
    
    // ... 测试代码
    
    // 清理资源
    delete it;
    delete db;
}
```

### 3. 断言使用
使用合适的断言：
- `ASSERT_*`: 失败时立即终止测试
- `EXPECT_*`: 失败时继续执行测试

### 4. 测试命名
使用清晰的测试名称：
```cpp
TEST(ClassName, TestCaseName) {
    // TestCaseName应该描述测试的内容
    // 例如: PutGetBasic, GetNotFound, WriteBatch
}
```

## 回归测试

### 性能回归检查
1. 运行性能基准测试
2. 对比历史性能数据
3. 识别性能下降
4. 报告性能问题

### 功能回归检查
1. 运行所有单元测试
2. 确保所有测试通过
3. 检查新增功能是否破坏现有功能
4. 验证向后兼容性
