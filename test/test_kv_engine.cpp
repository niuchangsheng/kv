#include "kv_engine.h"
#include <gtest/gtest.h>
#include <vector>
#include <map>

class KVEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Each test gets a fresh database instance
        options_.create_if_missing = true;
    }

    void TearDown() override {
        // Cleanup is handled by deleting the DB pointer
    }

    Options options_;
    WriteOptions write_options_;
    ReadOptions read_options_;
};

TEST_F(KVEngineTest, PutGet) {
    DB* db;
    Status status = DB::Open(options_, "/tmp/testdb_put_get", &db);
    ASSERT_TRUE(status.ok());
    
    // Test put
    status = db->Put(write_options_, "key1", "value1");
    ASSERT_TRUE(status.ok());
    
    // Test get
    std::string value;
    status = db->Get(read_options_, "key1", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "value1");
    
    // Test get non-existent key
    status = db->Get(read_options_, "nonexistent", &value);
    ASSERT_TRUE(status.IsNotFound());
    
    delete db;
}

TEST_F(KVEngineTest, Delete) {
    DB* db;
    Status status = DB::Open(options_, "/tmp/testdb_delete", &db);
    ASSERT_TRUE(status.ok());
    
    // Add a key
    status = db->Put(write_options_, "key1", "value1");
    ASSERT_TRUE(status.ok());
    
    // Verify it exists
    std::string value;
    status = db->Get(read_options_, "key1", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "value1");
    
    // Delete the key
    status = db->Delete(write_options_, "key1");
    ASSERT_TRUE(status.ok());
    
    // Verify it's gone
    status = db->Get(read_options_, "key1", &value);
    ASSERT_TRUE(status.IsNotFound());
    
    // Try to delete non-existent key (should not fail)
    status = db->Delete(write_options_, "nonexistent");
    ASSERT_TRUE(status.ok());
    
    delete db;
}

TEST_F(KVEngineTest, WriteBatch) {
    DB* db;
    Status status = DB::Open(options_, "/tmp/testdb_batch", &db);
    ASSERT_TRUE(status.ok());
    
    // Test batch operations
    WriteBatch batch;
    batch.Put("batch_key1", "batch_value1");
    batch.Put("batch_key2", "batch_value2");
    batch.Delete("batch_key1");
    
    status = db->Write(write_options_, &batch);
    ASSERT_TRUE(status.ok());
    
    // Verify batch_key1 was deleted (never existed)
    std::string value;
    status = db->Get(read_options_, "batch_key1", &value);
    ASSERT_TRUE(status.IsNotFound());
    
    // Verify batch_key2 exists
    status = db->Get(read_options_, "batch_key2", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "batch_value2");
    
    delete db;
}

TEST_F(KVEngineTest, Iterator) {
    DB* db;
    Status status = DB::Open(options_, "/tmp/testdb_iterator", &db);
    ASSERT_TRUE(status.ok());
    
    // Add some data
    db->Put(write_options_, "key1", "value1");
    db->Put(write_options_, "key2", "value2");
    db->Put(write_options_, "key3", "value3");
    
    // Test iterator
    Iterator* it = db->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    // Iterate through all keys (should be sorted)
    std::vector<std::pair<std::string, std::string>> results;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        results.push_back({it->key(), it->value()});
    }
    ASSERT_TRUE(it->status().ok()); // Should not be an error
    
    // Verify we got all keys in sorted order
    ASSERT_EQ(results.size(), 3);
    ASSERT_EQ(results[0].first, "key1");
    ASSERT_EQ(results[0].second, "value1");
    ASSERT_EQ(results[1].first, "key2");
    ASSERT_EQ(results[1].second, "value2");
    ASSERT_EQ(results[2].first, "key3");
    ASSERT_EQ(results[2].second, "value3");
    
    delete it;
    delete db;
}

TEST_F(KVEngineTest, MultipleValues) {
    DB* db;
    Status status = DB::Open(options_, "/tmp/testdb_multi", &db);
    ASSERT_TRUE(status.ok());
    
    // Add multiple key-value pairs
    for (int i = 0; i < 100; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        status = db->Put(write_options_, key, value);
        ASSERT_TRUE(status.ok());
    }
    
    // Verify all values
    for (int i = 0; i < 100; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expected_value = "value" + std::to_string(i);
        std::string actual_value;
        
        status = db->Get(read_options_, key, &actual_value);
        ASSERT_TRUE(status.ok());
        ASSERT_EQ(actual_value, expected_value);
    }
    
    delete db;
}

class DataCorrectnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        options_.create_if_missing = true;
        Status status = DB::Open(options_, "/tmp/testdb_correctness", &db_);
        ASSERT_TRUE(status.ok());
    }

    void TearDown() override {
        delete db_;
    }

    Options options_;
    WriteOptions write_options_;
    ReadOptions read_options_;
    DB* db_;
};

TEST_F(DataCorrectnessTest, BasicWriteRead) {
    // Test 1: Basic write-read correctness
    Status status = db_->Put(write_options_, "test_key", "test_value");
    ASSERT_TRUE(status.ok());

    std::string value;
    status = db_->Get(read_options_, "test_key", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "test_value");
}

TEST_F(DataCorrectnessTest, DataUpdate) {
    // Test 2: Data update correctness
    Status status = db_->Put(write_options_, "test_key", "test_value");
    ASSERT_TRUE(status.ok());
    
    status = db_->Put(write_options_, "test_key", "updated_value");
    ASSERT_TRUE(status.ok());

    std::string value;
    status = db_->Get(read_options_, "test_key", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "updated_value");
    ASSERT_NE(value, "test_value");  // Ensure old value is replaced
}

TEST_F(DataCorrectnessTest, EmptyValue) {
    // Test empty value
    Status status = db_->Put(write_options_, "empty_value_key", "");
    ASSERT_TRUE(status.ok());
    
    std::string value;
    status = db_->Get(read_options_, "empty_value_key", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "");
}

TEST_F(DataCorrectnessTest, SpecialCharacters) {
    // Test special characters
    std::string special_key = "key_with_special_chars_!@#$%^&*()";
    std::string special_value = "value_with\n\t\r\0\x01\xff";
    Status status = db_->Put(write_options_, special_key, special_value);
    ASSERT_TRUE(status.ok());
    
    std::string value;
    status = db_->Get(read_options_, special_key, &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, special_value);
}

TEST_F(DataCorrectnessTest, UnicodeCharacters) {
    // Test unicode characters (UTF-8)
    std::string unicode_key = "unicode_key_中文_日本語_한국어";
    std::string unicode_value = "unicode_value_测试_テスト_테스트";
    Status status = db_->Put(write_options_, unicode_key, unicode_value);
    ASSERT_TRUE(status.ok());
    
    std::string value;
    status = db_->Get(read_options_, unicode_key, &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, unicode_value);
}

TEST_F(DataCorrectnessTest, LargeData) {
    // Test large data (10KB)
    std::string large_key = "large_key";
    std::string large_value(10000, 'A');
    Status status = db_->Put(write_options_, large_key, large_value);
    ASSERT_TRUE(status.ok());
    
    std::string value;
    status = db_->Get(read_options_, large_key, &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, large_value);
    ASSERT_EQ(value.size(), 10000);
}

TEST_F(DataCorrectnessTest, VeryLargeData) {
    // Test very large value (100KB)
    std::string very_large_key = "very_large_key";
    std::string very_large_value(100000, 'B');
    Status status = db_->Put(write_options_, very_large_key, very_large_value);
    ASSERT_TRUE(status.ok());
    
    std::string value;
    status = db_->Get(read_options_, very_large_key, &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, very_large_value);
    ASSERT_EQ(value.size(), 100000);
}

TEST_F(DataCorrectnessTest, MultipleKeysSameValue) {
    // Test multiple keys with same value
    std::string shared_value = "shared_value";
    for (int i = 0; i < 10; ++i) {
        std::string key = "shared_key" + std::to_string(i);
        Status status = db_->Put(write_options_, key, shared_value);
        ASSERT_TRUE(status.ok());
    }
    
    // Verify all keys have the same value
    for (int i = 0; i < 10; ++i) {
        std::string key = "shared_key" + std::to_string(i);
        std::string value;
        Status status = db_->Get(read_options_, key, &value);
        ASSERT_TRUE(status.ok());
        ASSERT_EQ(value, shared_value);
    }
}

TEST_F(DataCorrectnessTest, DataIsolation) {
    // Test data isolation (keys don't interfere)
    Status status = db_->Put(write_options_, "key1", "value1");
    ASSERT_TRUE(status.ok());
    status = db_->Put(write_options_, "key2", "value2");
    ASSERT_TRUE(status.ok());
    status = db_->Put(write_options_, "key3", "value3");
    ASSERT_TRUE(status.ok());

    // Verify each key has its own value
    std::string value;
    status = db_->Get(read_options_, "key1", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "value1");

    status = db_->Get(read_options_, "key2", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "value2");

    status = db_->Get(read_options_, "key3", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "value3");
}

TEST_F(DataCorrectnessTest, WriteBatchCorrectness) {
    // Test WriteBatch data correctness
    WriteBatch batch;
    batch.Put("batch_key1", "batch_value1");
    batch.Put("batch_key2", "batch_value2");
    batch.Put("batch_key3", "batch_value3");
    
    Status status = db_->Write(write_options_, &batch);
    ASSERT_TRUE(status.ok());

    // Verify all batch writes are correct
    std::string value;
    status = db_->Get(read_options_, "batch_key1", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "batch_value1");

    status = db_->Get(read_options_, "batch_key2", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "batch_value2");

    status = db_->Get(read_options_, "batch_key3", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "batch_value3");
}

TEST_F(DataCorrectnessTest, DataPersistence) {
    // Test data persistence across operations
    Status status = db_->Put(write_options_, "persist_key", "persist_value");
    ASSERT_TRUE(status.ok());

    // Perform other operations
    status = db_->Put(write_options_, "other_key1", "other_value1");
    ASSERT_TRUE(status.ok());
    status = db_->Put(write_options_, "other_key2", "other_value2");
    ASSERT_TRUE(status.ok());
    status = db_->Delete(write_options_, "other_key1");
    ASSERT_TRUE(status.ok());

    // Verify original data is still correct
    std::string value;
    status = db_->Get(read_options_, "persist_key", &value);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(value, "persist_value");
}

TEST_F(DataCorrectnessTest, OverwriteCorrectness) {
    // Test overwrite correctness
    Status status = db_->Put(write_options_, "overwrite_key", "original_value");
    ASSERT_TRUE(status.ok());
    
    // Overwrite multiple times
    for (int i = 0; i < 5; ++i) {
        std::string new_value = "updated_value_" + std::to_string(i);
        status = db_->Put(write_options_, "overwrite_key", new_value);
        ASSERT_TRUE(status.ok());
        
        std::string value;
        status = db_->Get(read_options_, "overwrite_key", &value);
        ASSERT_TRUE(status.ok());
        ASSERT_EQ(value, new_value);
    }
}

TEST_F(DataCorrectnessTest, IteratorDataCorrectness) {
    // Test iterator data correctness
    // Add known data
    std::map<std::string, std::string> test_data = {
        {"a_key", "a_value"},
        {"b_key", "b_value"},
        {"c_key", "c_value"}
    };
    
    for (const auto& pair : test_data) {
        Status status = db_->Put(write_options_, pair.first, pair.second);
        ASSERT_TRUE(status.ok());
    }

    // Verify via iterator
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    std::map<std::string, std::string> iterated_data;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        iterated_data[it->key()] = it->value();
    }
    delete it;

    // Verify all data is correct
    for (const auto& pair : test_data) {
        auto found = iterated_data.find(pair.first);
        ASSERT_NE(found, iterated_data.end());
        ASSERT_EQ(found->second, pair.second);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
