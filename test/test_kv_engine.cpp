#include "kv_engine.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>
#include <cstdlib>

void test_put_get() {
    std::cout << "Testing put/get..." << std::endl;

    // Open database
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb_put_get", &db);
    assert(status.ok());
    
    // Test put
    WriteOptions write_options;
    status = db->Put(write_options, "key1", "value1");
    assert(status.ok());
    
    // Test get
    ReadOptions read_options;
    std::string value;
    status = db->Get(read_options, "key1", &value);
    assert(status.ok());
    assert(value == "value1");
    
    // Test get non-existent key
    status = db->Get(read_options, "nonexistent", &value);
    assert(status.IsNotFound());
    
    delete db;
    std::cout << "Put/get test passed!" << std::endl;
}

void test_delete() {
    std::cout << "Testing delete..." << std::endl;

    // Open database
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb_delete", &db);
    assert(status.ok());

    WriteOptions write_options;
    ReadOptions read_options;
    
    // Add a key
    status = db->Put(write_options, "key1", "value1");
    assert(status.ok());
    
    // Verify it exists
    std::string value;
    status = db->Get(read_options, "key1", &value);
    assert(status.ok());
    assert(value == "value1");
    
    // Delete the key
    status = db->Delete(write_options, "key1");
    assert(status.ok());
    
    // Verify it's gone
    status = db->Get(read_options, "key1", &value);
    assert(status.IsNotFound());

    // Try to delete non-existent key (should not fail)
    status = db->Delete(write_options, "nonexistent");
    assert(status.ok());

    delete db;
    std::cout << "Delete test passed!" << std::endl;
}

void test_write_batch() {
    std::cout << "Testing write batch..." << std::endl;

    // Open database
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb_batch", &db);
    assert(status.ok());

    WriteOptions write_options;
    ReadOptions read_options;

    // Test batch operations
    WriteBatch batch;
    batch.Put("batch_key1", "batch_value1");
    batch.Put("batch_key2", "batch_value2");
    batch.Delete("batch_key1");

    status = db->Write(write_options, &batch);
    assert(status.ok());

    // Verify batch_key1 was deleted (never existed)
    std::string value;
    status = db->Get(read_options, "batch_key1", &value);
    assert(status.IsNotFound());

    // Verify batch_key2 exists
    status = db->Get(read_options, "batch_key2", &value);
    assert(status.ok());
    assert(value == "batch_value2");

    delete db;
    std::cout << "Write batch test passed!" << std::endl;
}

void test_iterator() {
    std::cout << "Testing iterator..." << std::endl;

    // Open database
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb_iterator", &db);
    assert(status.ok());

    WriteOptions write_options;
    ReadOptions read_options;

    // Add some data
    db->Put(write_options, "key1", "value1");
    db->Put(write_options, "key2", "value2");
    db->Put(write_options, "key3", "value3");

    // Test iterator
    Iterator* it = db->NewIterator(read_options);
    assert(it != nullptr);

    // Iterate through all keys (should be sorted)
    std::vector<std::pair<std::string, std::string>> results;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        results.push_back({it->key(), it->value()});
    }
    assert(!it->status().IsNotFound()); // Should not be an error

    // Verify we got all keys in sorted order
    assert(results.size() == 3);
    assert(results[0].first == "key1" && results[0].second == "value1");
    assert(results[1].first == "key2" && results[1].second == "value2");
    assert(results[2].first == "key3" && results[2].second == "value3");

    delete it;
    delete db;
    std::cout << "Iterator test passed!" << std::endl;
}

void test_multiple_values() {
    std::cout << "Testing multiple values..." << std::endl;

    // Open database
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb_multi", &db);
    assert(status.ok());

    WriteOptions write_options;
    ReadOptions read_options;
    
    // Add multiple key-value pairs
    for (int i = 0; i < 100; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        status = db->Put(write_options, key, value);
        assert(status.ok());
    }
    
    // Verify all values
    for (int i = 0; i < 100; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expected_value = "value" + std::to_string(i);
        std::string actual_value;
        
        status = db->Get(read_options, key, &actual_value);
        assert(status.ok());
        assert(actual_value == expected_value);
    }
    
    delete db;
    std::cout << "Multiple values test passed!" << std::endl;
}

void test_data_correctness() {
    std::cout << "Testing data correctness..." << std::endl;

    // Open database
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb_correctness", &db);
    assert(status.ok());

    WriteOptions write_options;
    ReadOptions read_options;

    // Test 1: Basic write-read correctness
    std::cout << "  Test 1: Basic write-read correctness..." << std::endl;
    status = db->Put(write_options, "test_key", "test_value");
    assert(status.ok());

    std::string value;
    status = db->Get(read_options, "test_key", &value);
    assert(status.ok());
    assert(value == "test_value");
    std::cout << "    ✓ Basic write-read passed" << std::endl;

    // Test 2: Data update correctness
    std::cout << "  Test 2: Data update correctness..." << std::endl;
    status = db->Put(write_options, "test_key", "updated_value");
    assert(status.ok());

    status = db->Get(read_options, "test_key", &value);
    assert(status.ok());
    assert(value == "updated_value");
    assert(value != "test_value");  // Ensure old value is replaced
    std::cout << "    ✓ Data update passed" << std::endl;

    // Test 3: Special characters and edge cases
    std::cout << "  Test 3: Special characters and edge cases..." << std::endl;
    
    // Test empty value
    status = db->Put(write_options, "empty_value_key", "");
    assert(status.ok());
    status = db->Get(read_options, "empty_value_key", &value);
    assert(status.ok());
    assert(value == "");
    std::cout << "    ✓ Empty value passed" << std::endl;

    // Test special characters
    std::string special_key = "key_with_special_chars_!@#$%^&*()";
    std::string special_value = "value_with\n\t\r\0\x01\xff";
    status = db->Put(write_options, special_key, special_value);
    assert(status.ok());
    status = db->Get(read_options, special_key, &value);
    assert(status.ok());
    assert(value == special_value);
    std::cout << "    ✓ Special characters passed" << std::endl;

    // Test unicode characters (UTF-8)
    std::string unicode_key = "unicode_key_中文_日本語_한국어";
    std::string unicode_value = "unicode_value_测试_テスト_테스트";
    status = db->Put(write_options, unicode_key, unicode_value);
    assert(status.ok());
    status = db->Get(read_options, unicode_key, &value);
    assert(status.ok());
    assert(value == unicode_value);
    std::cout << "    ✓ Unicode characters passed" << std::endl;

    // Test 4: Large data correctness
    std::cout << "  Test 4: Large data correctness..." << std::endl;
    std::string large_key = "large_key";
    std::string large_value(10000, 'A');  // 10KB value
    status = db->Put(write_options, large_key, large_value);
    assert(status.ok());
    
    status = db->Get(read_options, large_key, &value);
    assert(status.ok());
    assert(value == large_value);
    assert(value.size() == 10000);
    std::cout << "    ✓ Large data (10KB) passed" << std::endl;

    // Test very large value
    std::string very_large_key = "very_large_key";
    std::string very_large_value(100000, 'B');  // 100KB value
    status = db->Put(write_options, very_large_key, very_large_value);
    assert(status.ok());
    
    status = db->Get(read_options, very_large_key, &value);
    assert(status.ok());
    assert(value == very_large_value);
    assert(value.size() == 100000);
    std::cout << "    ✓ Very large data (100KB) passed" << std::endl;

    // Test 5: Multiple keys with same value
    std::cout << "  Test 5: Multiple keys with same value..." << std::endl;
    std::string shared_value = "shared_value";
    for (int i = 0; i < 10; ++i) {
        std::string key = "shared_key" + std::to_string(i);
        status = db->Put(write_options, key, shared_value);
        assert(status.ok());
    }
    
    // Verify all keys have the same value
    for (int i = 0; i < 10; ++i) {
        std::string key = "shared_key" + std::to_string(i);
        status = db->Get(read_options, key, &value);
        assert(status.ok());
        assert(value == shared_value);
    }
    std::cout << "    ✓ Multiple keys with same value passed" << std::endl;

    // Test 6: Data isolation (keys don't interfere)
    std::cout << "  Test 6: Data isolation..." << std::endl;
    status = db->Put(write_options, "key1", "value1");
    assert(status.ok());
    status = db->Put(write_options, "key2", "value2");
    assert(status.ok());
    status = db->Put(write_options, "key3", "value3");
    assert(status.ok());

    // Verify each key has its own value
    status = db->Get(read_options, "key1", &value);
    assert(status.ok());
    assert(value == "value1");

    status = db->Get(read_options, "key2", &value);
    assert(status.ok());
    assert(value == "value2");

    status = db->Get(read_options, "key3", &value);
    assert(status.ok());
    assert(value == "value3");
    std::cout << "    ✓ Data isolation passed" << std::endl;

    // Test 7: WriteBatch data correctness
    std::cout << "  Test 7: WriteBatch data correctness..." << std::endl;
    WriteBatch batch;
    batch.Put("batch_key1", "batch_value1");
    batch.Put("batch_key2", "batch_value2");
    batch.Put("batch_key3", "batch_value3");
    
    status = db->Write(write_options, &batch);
    assert(status.ok());

    // Verify all batch writes are correct
    status = db->Get(read_options, "batch_key1", &value);
    assert(status.ok());
    assert(value == "batch_value1");

    status = db->Get(read_options, "batch_key2", &value);
    assert(status.ok());
    assert(value == "batch_value2");

    status = db->Get(read_options, "batch_key3", &value);
    assert(status.ok());
    assert(value == "batch_value3");
    std::cout << "    ✓ WriteBatch data correctness passed" << std::endl;

    // Test 8: Data persistence across operations
    std::cout << "  Test 8: Data persistence across operations..." << std::endl;
    status = db->Put(write_options, "persist_key", "persist_value");
    assert(status.ok());

    // Perform other operations
    status = db->Put(write_options, "other_key1", "other_value1");
    assert(status.ok());
    status = db->Put(write_options, "other_key2", "other_value2");
    assert(status.ok());
    status = db->Delete(write_options, "other_key1");
    assert(status.ok());

    // Verify original data is still correct
    status = db->Get(read_options, "persist_key", &value);
    assert(status.ok());
    assert(value == "persist_value");
    std::cout << "    ✓ Data persistence passed" << std::endl;

    // Test 9: Overwrite correctness
    std::cout << "  Test 9: Overwrite correctness..." << std::endl;
    status = db->Put(write_options, "overwrite_key", "original_value");
    assert(status.ok());
    
    // Overwrite multiple times
    for (int i = 0; i < 5; ++i) {
        std::string new_value = "updated_value_" + std::to_string(i);
        status = db->Put(write_options, "overwrite_key", new_value);
        assert(status.ok());
        
        status = db->Get(read_options, "overwrite_key", &value);
        assert(status.ok());
        assert(value == new_value);
    }
    std::cout << "    ✓ Overwrite correctness passed" << std::endl;

    // Test 10: Iterator data correctness
    std::cout << "  Test 10: Iterator data correctness..." << std::endl;
    // Clear and add test data
    db->Delete(write_options, "test_key");
    db->Delete(write_options, "empty_value_key");
    db->Delete(write_options, special_key);
    db->Delete(write_options, unicode_key);
    db->Delete(write_options, large_key);
    db->Delete(write_options, very_large_key);
    
    // Add known data
    std::map<std::string, std::string> test_data = {
        {"a_key", "a_value"},
        {"b_key", "b_value"},
        {"c_key", "c_value"}
    };
    
    for (const auto& pair : test_data) {
        status = db->Put(write_options, pair.first, pair.second);
        assert(status.ok());
    }

    // Verify via iterator
    Iterator* it = db->NewIterator(read_options);
    assert(it != nullptr);
    
    std::map<std::string, std::string> iterated_data;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        iterated_data[it->key()] = it->value();
    }
    delete it;

    // Verify all data is correct
    for (const auto& pair : test_data) {
        auto found = iterated_data.find(pair.first);
        assert(found != iterated_data.end());
        assert(found->second == pair.second);
    }
    std::cout << "    ✓ Iterator data correctness passed" << std::endl;

    delete db;
    std::cout << "Data correctness test passed!" << std::endl;
}

void test_silent_data_corruption() {
    std::cout << "Testing silent data corruption (long-running test)..." << std::endl;
    std::cout << "This test runs continuously to detect data corruption over time." << std::endl;

    // Open database
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb_silent_corruption", &db);
    assert(status.ok());

    WriteOptions write_options;
    ReadOptions read_options;

    // Test configuration (can be overridden by environment variables)
    const char* duration_env = std::getenv("KV_TEST_DURATION_SECONDS");
    const int test_duration_seconds = duration_env ? std::atoi(duration_env) : 30;
    
    const char* keys_env = std::getenv("KV_TEST_NUM_KEYS");
    const int num_keys = keys_env ? std::atoi(keys_env) : 1000;
    
    const char* interval_env = std::getenv("KV_TEST_VERIFY_INTERVAL_MS");
    const int verification_interval_ms = interval_env ? std::atoi(interval_env) : 100;
    
    std::cout << "  Configuration:" << std::endl;
    std::cout << "    Keys: " << num_keys << std::endl;
    std::cout << "    Duration: " << test_duration_seconds << " seconds" << std::endl;
    std::cout << "    Verification interval: " << verification_interval_ms << " ms" << std::endl;
    std::cout << std::endl;

    // Data structure to track expected values
    std::map<std::string, std::string> expected_data;
    std::map<std::string, int> write_count;  // Track how many times each key was written
    
    // Statistics
    int total_writes = 0;
    int total_reads = 0;
    int total_verifications = 0;
    int corruption_errors = 0;
    int not_found_errors = 0;
    int other_errors = 0;

    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(0, num_keys - 1);
    std::uniform_int_distribution<> value_dist(1, 1000);

    auto start_time = std::chrono::steady_clock::now();
    auto last_verification = start_time;
    auto last_status_report = start_time;

    std::cout << "  Starting continuous read/write test..." << std::endl;

    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time).count();

        // Check if test duration has elapsed
        if (elapsed >= test_duration_seconds) {
            break;
        }

        // Random operation: 70% write, 30% read
        int operation = value_dist(gen) % 10;
        
        if (operation < 7) {
            // Write operation
            int key_index = key_dist(gen);
            std::string key = "key_" + std::to_string(key_index);
            std::string value = "value_" + std::to_string(value_dist(gen)) + "_" + 
                               std::to_string(total_writes);
            
            status = db->Put(write_options, key, value);
            if (!status.ok()) {
                std::cerr << "  ERROR: Write failed for key " << key 
                         << ": " << status.ToString() << std::endl;
                other_errors++;
                continue;
            }

            // Update expected data
            expected_data[key] = value;
            write_count[key]++;
            total_writes++;
        } else {
            // Read operation
            int key_index = key_dist(gen);
            std::string key = "key_" + std::to_string(key_index);
            
            std::string actual_value;
            status = db->Get(read_options, key, &actual_value);
            total_reads++;

            if (status.IsNotFound()) {
                // Key might not exist yet, that's OK
                if (expected_data.find(key) != expected_data.end()) {
                    // But we expected it to exist!
                    std::cerr << "  ERROR: Key " << key 
                             << " was written but now not found!" << std::endl;
                    not_found_errors++;
                }
            } else if (!status.ok()) {
                std::cerr << "  ERROR: Read failed for key " << key 
                         << ": " << status.ToString() << std::endl;
                other_errors++;
            } else {
                // Verify data correctness
                auto it = expected_data.find(key);
                if (it != expected_data.end()) {
                    if (actual_value != it->second) {
                        std::cerr << "  CORRUPTION DETECTED!" << std::endl;
                        std::cerr << "    Key: " << key << std::endl;
                        std::cerr << "    Expected: " << it->second << std::endl;
                        std::cerr << "    Actual: " << actual_value << std::endl;
                        std::cerr << "    Write count: " << write_count[key] << std::endl;
                        corruption_errors++;
                    }
                }
            }
        }

        // Periodic verification: check all expected keys
        auto time_since_verification = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_verification).count();
        
        if (time_since_verification >= verification_interval_ms) {
            // Verify all keys in expected_data
            for (const auto& pair : expected_data) {
                std::string actual_value;
                status = db->Get(read_options, pair.first, &actual_value);
                
                if (status.IsNotFound()) {
                    std::cerr << "  CORRUPTION: Key " << pair.first 
                             << " was written but now not found!" << std::endl;
                    not_found_errors++;
                } else if (!status.ok()) {
                    std::cerr << "  ERROR: Read failed for key " << pair.first 
                             << ": " << status.ToString() << std::endl;
                    other_errors++;
                } else if (actual_value != pair.second) {
                    std::cerr << "  CORRUPTION DETECTED!" << std::endl;
                    std::cerr << "    Key: " << pair.first << std::endl;
                    std::cerr << "    Expected: " << pair.second << std::endl;
                    std::cerr << "    Actual: " << actual_value << std::endl;
                    std::cerr << "    Write count: " << write_count[pair.first] << std::endl;
                    corruption_errors++;
                }
            }
            total_verifications++;
            last_verification = current_time;
        }

        // Periodic status report
        auto time_since_report = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_status_report).count();
        
        if (time_since_report >= 5) {
            std::cout << "  [" << elapsed << "s] Writes: " << total_writes 
                     << ", Reads: " << total_reads 
                     << ", Verifications: " << total_verifications
                     << ", Errors: " << (corruption_errors + not_found_errors + other_errors)
                     << std::endl;
            last_status_report = current_time;
        }

        // Small delay to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Final comprehensive verification
    std::cout << std::endl;
    std::cout << "  Performing final comprehensive verification..." << std::endl;
    int final_verification_errors = 0;
    
    for (const auto& pair : expected_data) {
        std::string actual_value;
        status = db->Get(read_options, pair.first, &actual_value);
        
        if (status.IsNotFound()) {
            std::cerr << "  FINAL ERROR: Key " << pair.first 
                     << " was written but now not found!" << std::endl;
            final_verification_errors++;
        } else if (!status.ok()) {
            std::cerr << "  FINAL ERROR: Read failed for key " << pair.first 
                     << ": " << status.ToString() << std::endl;
            final_verification_errors++;
        } else if (actual_value != pair.second) {
            std::cerr << "  FINAL CORRUPTION DETECTED!" << std::endl;
            std::cerr << "    Key: " << pair.first << std::endl;
            std::cerr << "    Expected: " << pair.second << std::endl;
            std::cerr << "    Actual: " << actual_value << std::endl;
            final_verification_errors++;
        }
    }

    // Print statistics
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();

    std::cout << std::endl;
    std::cout << "  Test Statistics:" << std::endl;
    std::cout << "    Total time: " << total_time << " ms" << std::endl;
    std::cout << "    Total writes: " << total_writes << std::endl;
    std::cout << "    Total reads: " << total_reads << std::endl;
    std::cout << "    Total verifications: " << total_verifications << std::endl;
    std::cout << "    Keys tracked: " << expected_data.size() << std::endl;
    std::cout << std::endl;
    std::cout << "  Error Statistics:" << std::endl;
    std::cout << "    Data corruption errors: " << corruption_errors << std::endl;
    std::cout << "    Not found errors: " << not_found_errors << std::endl;
    std::cout << "    Other errors: " << other_errors << std::endl;
    std::cout << "    Final verification errors: " << final_verification_errors << std::endl;
    std::cout << std::endl;

    // Assert no errors
    if (corruption_errors > 0 || not_found_errors > 0 || other_errors > 0 || final_verification_errors > 0) {
        std::cerr << "  FAILED: Data corruption or errors detected!" << std::endl;
        std::cerr << "  Total errors: " 
                 << (corruption_errors + not_found_errors + other_errors + final_verification_errors) 
                 << std::endl;
        assert(false);
    }

    std::cout << "  ✓ No data corruption detected during " << test_duration_seconds 
             << " seconds of continuous operation" << std::endl;
    std::cout << "  ✓ All " << expected_data.size() << " keys verified correct" << std::endl;

    delete db;
    std::cout << "Silent data corruption test passed!" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "Running KV Engine tests..." << std::endl;
    std::cout << std::endl;
    
    // Run standard tests
    test_put_get();
    test_delete();
    test_write_batch();
    test_iterator();
    test_multiple_values();
    test_data_correctness();

    // Run long-running silent corruption test if requested
    bool run_long_test = false;
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--long" || arg == "-l" || arg == "--silent-corruption") {
            run_long_test = true;
        }
    }

    if (run_long_test) {
        std::cout << std::endl;
        test_silent_data_corruption();
    } else {
        std::cout << std::endl;
        std::cout << "Note: Silent corruption test skipped (use --long to run)" << std::endl;
        std::cout << "  Run with: ./bin/test_kv_engine --long" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "All tests passed!" << std::endl;
    
    return 0;
}