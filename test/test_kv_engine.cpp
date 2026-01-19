#include "kv_engine.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <map>

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

int main() {
    std::cout << "Running KV Engine tests..." << std::endl;
    std::cout << std::endl;

    test_put_get();
    test_delete();
    test_write_batch();
    test_iterator();
    test_multiple_values();
    test_data_correctness();

    std::cout << std::endl;
    std::cout << "All tests passed!" << std::endl;

    return 0;
}