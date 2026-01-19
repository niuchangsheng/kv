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
   └─► 执行单元测试 (make test)
   └─► 生成覆盖率报告 (./scripts/generate_coverage.sh)
   └─► 检查代码覆盖率是否>80%
   
4. 性能测试
   └─► 运行性能基准测试 (./bin/kv_bench)
   └─► 对比性能指标
   └─► 检测性能回归
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

### 功能测试
- [ ] 是否覆盖了正常路径？
- [ ] 是否覆盖了错误路径？
- [ ] 是否覆盖了边界条件？
- [ ] 测试是否能够通过？
- [ ] 是否测试了资源清理？

### 代码覆盖率
- [ ] 是否运行了覆盖率报告生成脚本？(`./scripts/generate_coverage.sh`)
- [ ] 代码覆盖率是否>80%？
- [ ] 是否查看了覆盖率报告，识别未覆盖的代码？
- [ ] 是否为未覆盖的代码添加了测试用例？

### 性能测试
- [ ] 是否运行了性能测试？(`./bin/kv_bench`)
- [ ] 是否记录了性能基准数据？
- [ ] 是否进行了性能回归检查？
- [ ] 性能指标是否在可接受范围内？

### 数据可靠性测试
- [ ] 是否运行了数据正确性测试？（包含在`make test`中）
- [ ] 是否运行了长时间运行的数据正确性测试？(`./bin/test_data_correctness_long`)
- [ ] 长时间测试是否没有发现数据损坏？
- [ ] 是否在关键变更后运行了长时间测试？

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

### 长时间运行测试（数据正确性）
- **目标**: 检测数据静默损坏（Silent Data Corruption）
- **场景**: 数据在写入时读取正确，但过一段时间后可能损坏
- **方法**: 持续运行读写操作，定期验证之前写入的数据
- **执行**: `./bin/test_data_correctness_long`（独立测试程序）
- **配置**: 通过环境变量控制
  - `KV_TEST_DURATION_SECONDS`: 测试持续时间（默认30秒）
  - `KV_TEST_NUM_KEYS`: 维护的键数量（默认1000）
  - `KV_TEST_VERIFY_INTERVAL_MS`: 验证间隔（默认100ms）
- **示例**:
  ```bash
  # 运行30秒的长时间测试（默认）
  ./bin/test_data_correctness_long
  
  # 运行3秒的快速测试
  KV_TEST_DURATION_SECONDS=3 ./bin/test_data_correctness_long
  
  # 运行60秒的长时间测试，使用2000个键
  KV_TEST_DURATION_SECONDS=60 KV_TEST_NUM_KEYS=2000 ./bin/test_data_correctness_long
  ```
- **验证内容**:
  - 持续写入和读取数据
  - 定期验证所有已写入的数据是否正确
  - 检测数据是否被静默损坏
  - 统计错误数量和类型

## 代码覆盖率要求

### 覆盖率目标
- **总体覆盖率**: >80%
- **关键路径**: 100%
- **错误处理**: >90%

### 覆盖率检查

#### 使用自动化脚本（推荐）

项目提供了自动化脚本 `scripts/generate_coverage.sh` 来生成覆盖率报告：

```bash
# 从项目根目录运行
./scripts/generate_coverage.sh

# 生成报告并自动在浏览器中打开
./scripts/generate_coverage.sh --open
```

**脚本功能**:
- ✅ 自动检查依赖工具（gcov, lcov）
- ✅ 清理旧的构建文件
- ✅ 使用覆盖率标志编译项目
- ✅ 运行测试程序
- ✅ 生成覆盖率数据
- ✅ 过滤test目录和系统库（只统计src目录）
- ✅ 生成HTML报告
- ✅ 显示覆盖率摘要

**输出**:
- **覆盖率报告**: `build/coverage/index.html`
- **过滤后的数据**: `build/coverage_filtered.info`
- **覆盖率摘要**: 终端输出（行覆盖率、函数覆盖率）

#### 手动生成（高级用法）

如果需要手动控制每个步骤：

```bash
# 1. 使用覆盖率标志编译
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage -g -O0"
make

# 2. 运行测试
make test

# 3. 生成覆盖率数据
lcov --capture --directory . --output-file coverage.info --ignore-errors source

# 4. 过滤数据（排除test和系统库）
lcov --remove coverage.info '/usr/*' '*/test/*' '*/build/*' \
     --output-file coverage_filtered.info --ignore-errors unused,empty

# 5. 生成HTML报告
genhtml coverage_filtered.info --output-directory coverage \
        --title "KV Engine Coverage Report (src only)" \
        --show-details --legend
```

### 覆盖率报告解读

生成的HTML报告包含：
- **总体覆盖率**: 行覆盖率、函数覆盖率、分支覆盖率
- **文件级覆盖率**: 每个源文件的详细覆盖率
- **行级覆盖率**: 每行代码的覆盖情况（绿色=已覆盖，红色=未覆盖）
- **分支覆盖率**: 条件分支的覆盖情况

**查看报告**:
```bash
# Linux
xdg-open build/coverage/index.html

# macOS
open build/coverage/index.html
```

## 性能测试

### 使用 kv_bench 性能测试工具

项目提供了 `kv_bench` 工具用于性能测试和回归检测。

#### 运行性能测试

```bash
# 从项目根目录运行
cd build

# 使用默认参数（10000 operations, 16-byte keys, 64-byte values）
./bin/kv_bench

# 自定义参数
./bin/kv_bench <operations> <key_size> <value_size>
./bin/kv_bench 1000 32 128
```

#### 性能测试内容

`kv_bench` 会执行以下性能测试：

1. **Put Benchmark**: 测试写入操作性能
   - 吞吐量（ops/sec）
   - 平均延迟（us）

2. **Get Benchmark**: 测试读取操作性能
   - 吞吐量（ops/sec）
   - 平均延迟（us）

3. **Delete Benchmark**: 测试删除操作性能
   - 吞吐量（ops/sec）
   - 平均延迟（us）

4. **WriteBatch Benchmark**: 测试批量写入性能
   - 批量操作吞吐量
   - 平均延迟

5. **Iterator Benchmark**: 测试迭代器遍历性能
   - 遍历所有键的平均延迟

#### 性能测试输出示例

```
========================================
KV Engine Performance Benchmark
========================================

Configuration:
  Operations: 10000
  Key size: 16 bytes
  Value size: 64 bytes

Put Benchmark:
  Operations: 10000
  Key size: 16 bytes
  Value size: 64 bytes
  Total time: 7870 us
  Throughput: 1270648.03 ops/sec
  Avg latency: 0.79 us

Get Benchmark:
  Operations: 10000
  Key size: 16 bytes
  Value size: 64 bytes
  Total time: 2870 us
  Throughput: 3484320.56 ops/sec
  Avg latency: 0.29 us

...
```

#### 性能回归检测

**步骤**:
1. 运行性能测试并记录基准数据
2. 修改代码后重新运行测试
3. 对比性能指标变化
4. 识别性能回归（性能下降超过阈值）

**示例流程**:
```bash
# 1. 建立性能基准
cd build
./bin/kv_bench 10000 > baseline.txt

# 2. 修改代码后重新测试
./bin/kv_bench 10000 > current.txt

# 3. 对比结果
diff baseline.txt current.txt
# 或手动对比关键指标（吞吐量、延迟）
```

### 性能指标目标

- **Put操作延迟**: <1ms (内存版本)
- **Get操作延迟**: <1ms (内存版本)
- **Delete操作延迟**: <1ms (内存版本)
- **批量操作吞吐量**: >100K ops/s
- **迭代器遍历**: 每个键 <2us
- **内存使用**: 监控内存增长

### 性能测试最佳实践

1. **使用固定参数**: 使用相同的测试参数进行对比
2. **多次运行**: 运行多次取平均值，减少波动
3. **记录基准**: 保存性能基准数据用于对比
4. **监控趋势**: 跟踪性能指标的变化趋势
5. **设置阈值**: 定义性能回归的阈值（如延迟增加>10%）

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

使用 `kv_bench` 工具进行性能回归测试：

```bash
# 1. 建立性能基准（在代码修改前）
cd build
./bin/kv_bench 10000 > performance_baseline.txt

# 2. 修改代码后重新测试
./bin/kv_bench 10000 > performance_current.txt

# 3. 对比性能指标
# 重点关注：
# - 吞吐量变化（ops/sec）
# - 延迟变化（us）
# - 是否有明显的性能下降
```

**性能回归检查步骤**:
1. ✅ 运行 `kv_bench` 建立性能基准
2. ✅ 代码修改后重新运行 `kv_bench`
3. ✅ 对比关键性能指标（吞吐量、延迟）
4. ✅ 识别性能下降（如延迟增加>10%或吞吐量下降>5%）
5. ✅ 报告性能问题并分析原因

**性能回归阈值**:
- **延迟增加**: >10% 视为性能回归
- **吞吐量下降**: >5% 视为性能回归
- **内存使用**: 显著增加需要关注

### 功能回归检查

1. ✅ 运行所有单元测试 (`make test`)
2. ✅ 确保所有测试通过
3. ✅ 检查新增功能是否破坏现有功能
4. ✅ 验证向后兼容性
5. ✅ 检查代码覆盖率是否下降（运行 `./scripts/generate_coverage.sh`）
