#include "kv_engine.h"
#include <cassert>
#include <iostream>
#include <vector>

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

int main() {
    std::cout << "Running KV Engine tests..." << std::endl;

    test_put_get();
    test_delete();
    test_write_batch();
    test_iterator();
    test_multiple_values();

    std::cout << "All tests passed!" << std::endl;

    return 0;
}