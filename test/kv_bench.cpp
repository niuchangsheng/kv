#include "db.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <cstring>

// 性能测试工具类
class Benchmark {
public:
    static void RunPutBenchmark(DB* db, int num_operations, int key_size, int value_size) {
        WriteOptions write_options;
        std::vector<std::string> keys;
        std::vector<std::string> values;
        
        // 准备测试数据
        for (int i = 0; i < num_operations; ++i) {
            keys.push_back(GenerateRandomString(key_size));
            values.push_back(GenerateRandomString(value_size));
        }
        
        // 预热
        for (int i = 0; i < std::min(100, num_operations); ++i) {
            db->Put(write_options, keys[i], values[i]);
        }
        
        // 清空数据库
        for (int i = 0; i < std::min(100, num_operations); ++i) {
            db->Delete(write_options, keys[i]);
        }
        
        // 实际测试
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_operations; ++i) {
            db->Put(write_options, keys[i], values[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double ops_per_sec = (num_operations * 1000000.0) / duration.count();
        double avg_latency_us = duration.count() / (double)num_operations;
        
        std::cout << "Put Benchmark:" << std::endl;
        std::cout << "  Operations: " << num_operations << std::endl;
        std::cout << "  Key size: " << key_size << " bytes" << std::endl;
        std::cout << "  Value size: " << value_size << " bytes" << std::endl;
        std::cout << "  Total time: " << duration.count() << " us" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << ops_per_sec << " ops/sec" << std::endl;
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) 
                  << avg_latency_us << " us" << std::endl;
        std::cout << std::endl;
    }
    
    static void RunGetBenchmark(DB* db, int num_operations, int key_size, int value_size) {
        WriteOptions write_options;
        ReadOptions read_options;
        std::vector<std::string> keys;
        std::vector<std::string> values;
        
        // 准备测试数据
        for (int i = 0; i < num_operations; ++i) {
            keys.push_back(GenerateRandomString(key_size));
            values.push_back(GenerateRandomString(value_size));
            db->Put(write_options, keys[i], values[i]);
        }
        
        // 预热
        std::string dummy;
        for (int i = 0; i < std::min(100, num_operations); ++i) {
            db->Get(read_options, keys[i], &dummy);
        }
        
        // 实际测试
        auto start = std::chrono::high_resolution_clock::now();
        std::string value;
        for (int i = 0; i < num_operations; ++i) {
            db->Get(read_options, keys[i], &value);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double ops_per_sec = (num_operations * 1000000.0) / duration.count();
        double avg_latency_us = duration.count() / (double)num_operations;
        
        std::cout << "Get Benchmark:" << std::endl;
        std::cout << "  Operations: " << num_operations << std::endl;
        std::cout << "  Key size: " << key_size << " bytes" << std::endl;
        std::cout << "  Value size: " << value_size << " bytes" << std::endl;
        std::cout << "  Total time: " << duration.count() << " us" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << ops_per_sec << " ops/sec" << std::endl;
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) 
                  << avg_latency_us << " us" << std::endl;
        std::cout << std::endl;
    }
    
    static void RunDeleteBenchmark(DB* db, int num_operations, int key_size, int value_size) {
        WriteOptions write_options;
        std::vector<std::string> keys;
        std::vector<std::string> values;
        
        // 准备测试数据
        for (int i = 0; i < num_operations; ++i) {
            keys.push_back(GenerateRandomString(key_size));
            values.push_back(GenerateRandomString(value_size));
            db->Put(write_options, keys[i], values[i]);
        }
        
        // 实际测试
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_operations; ++i) {
            db->Delete(write_options, keys[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double ops_per_sec = (num_operations * 1000000.0) / duration.count();
        double avg_latency_us = duration.count() / (double)num_operations;
        
        std::cout << "Delete Benchmark:" << std::endl;
        std::cout << "  Operations: " << num_operations << std::endl;
        std::cout << "  Key size: " << key_size << " bytes" << std::endl;
        std::cout << "  Total time: " << duration.count() << " us" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << ops_per_sec << " ops/sec" << std::endl;
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) 
                  << avg_latency_us << " us" << std::endl;
        std::cout << std::endl;
    }
    
    static void RunWriteBatchBenchmark(DB* db, int num_batches, int batch_size) {
        WriteOptions write_options;
        
        // 准备测试数据
        std::vector<std::vector<std::string>> batch_keys(num_batches);
        std::vector<std::vector<std::string>> batch_values(num_batches);
        
        for (int i = 0; i < num_batches; ++i) {
            for (int j = 0; j < batch_size; ++j) {
                batch_keys[i].push_back("batch" + std::to_string(i) + "_key" + std::to_string(j));
                batch_values[i].push_back("batch" + std::to_string(i) + "_value" + std::to_string(j));
            }
        }
        
        // 实际测试
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_batches; ++i) {
            WriteBatch batch;
            for (int j = 0; j < batch_size; ++j) {
                batch.Put(batch_keys[i][j], batch_values[i][j]);
            }
            db->Write(write_options, &batch);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        int total_operations = num_batches * batch_size;
        double ops_per_sec = (total_operations * 1000000.0) / duration.count();
        double avg_latency_us = duration.count() / (double)total_operations;
        
        std::cout << "WriteBatch Benchmark:" << std::endl;
        std::cout << "  Batches: " << num_batches << std::endl;
        std::cout << "  Batch size: " << batch_size << std::endl;
        std::cout << "  Total operations: " << total_operations << std::endl;
        std::cout << "  Total time: " << duration.count() << " us" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << ops_per_sec << " ops/sec" << std::endl;
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) 
                  << avg_latency_us << " us" << std::endl;
        std::cout << std::endl;
    }
    
    static void RunIteratorBenchmark(DB* db, int num_keys) {
        WriteOptions write_options;
        ReadOptions read_options;
        
        // 准备测试数据
        for (int i = 0; i < num_keys; ++i) {
            std::string key = "key" + std::to_string(i);
            std::string value = "value" + std::to_string(i);
            db->Put(write_options, key, value);
        }
        
        // 实际测试
        auto start = std::chrono::high_resolution_clock::now();
        Iterator* it = db->NewIterator(read_options);
        int count = 0;
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            count++;
            // 访问key和value以确保实际读取
            std::string key = it->key();
            std::string value = it->value();
        }
        delete it;
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_latency_us = duration.count() / (double)count;
        
        std::cout << "Iterator Benchmark:" << std::endl;
        std::cout << "  Keys: " << num_keys << std::endl;
        std::cout << "  Iterated: " << count << " keys" << std::endl;
        std::cout << "  Total time: " << duration.count() << " us" << std::endl;
        std::cout << "  Avg latency per key: " << std::fixed << std::setprecision(2) 
                  << avg_latency_us << " us" << std::endl;
        std::cout << std::endl;
    }
    
private:
    static std::string GenerateRandomString(int length) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
        
        std::string result;
        result.reserve(length);
        for (int i = 0; i < length; ++i) {
            result += alphanum[dis(gen)];
        }
        return result;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "KV Engine Performance Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 打开数据库
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/kv_bench", &db);
    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.ToString() << std::endl;
        return 1;
    }
    
    // 解析命令行参数
    int num_operations = 10000;
    int key_size = 16;
    int value_size = 64;
    
    if (argc > 1) {
        num_operations = std::atoi(argv[1]);
    }
    if (argc > 2) {
        key_size = std::atoi(argv[2]);
    }
    if (argc > 3) {
        value_size = std::atoi(argv[3]);
    }
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Operations: " << num_operations << std::endl;
    std::cout << "  Key size: " << key_size << " bytes" << std::endl;
    std::cout << "  Value size: " << value_size << " bytes" << std::endl;
    std::cout << std::endl;
    
    // 运行性能测试
    Benchmark::RunPutBenchmark(db, num_operations, key_size, value_size);
    Benchmark::RunGetBenchmark(db, num_operations, key_size, value_size);
    Benchmark::RunDeleteBenchmark(db, num_operations, key_size, value_size);
    Benchmark::RunWriteBatchBenchmark(db, num_operations / 10, 10);
    Benchmark::RunIteratorBenchmark(db, num_operations / 10);
    
    std::cout << "========================================" << std::endl;
    std::cout << "Benchmark completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    delete db;
    return 0;
}
