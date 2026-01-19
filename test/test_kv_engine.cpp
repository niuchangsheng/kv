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

// Status class tests
class StatusTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(StatusTest, OKStatus) {
    Status status = Status::OK();
    ASSERT_TRUE(status.ok());
    ASSERT_FALSE(status.IsNotFound());
    ASSERT_FALSE(status.IsCorruption());
    ASSERT_FALSE(status.IsIOError());
    ASSERT_EQ(status.ToString(), "OK");
}

TEST_F(StatusTest, NotFoundStatus) {
    Status status = Status::NotFound("Key not found");
    ASSERT_FALSE(status.ok());
    ASSERT_TRUE(status.IsNotFound());
    ASSERT_FALSE(status.IsCorruption());
    ASSERT_FALSE(status.IsIOError());
    ASSERT_EQ(status.ToString(), "NotFound: Key not found");
    
    // Test default message
    Status status2 = Status::NotFound();
    ASSERT_TRUE(status2.IsNotFound());
    ASSERT_EQ(status2.ToString(), "NotFound: Not Found");
}

TEST_F(StatusTest, CorruptionStatus) {
    Status status = Status::Corruption("Data corrupted");
    ASSERT_FALSE(status.ok());
    ASSERT_FALSE(status.IsNotFound());
    ASSERT_TRUE(status.IsCorruption());
    ASSERT_FALSE(status.IsIOError());
    ASSERT_EQ(status.ToString(), "Corruption: Data corrupted");
    
    // Test default message
    Status status2 = Status::Corruption();
    ASSERT_TRUE(status2.IsCorruption());
    ASSERT_EQ(status2.ToString(), "Corruption: Corruption");
}

TEST_F(StatusTest, NotSupportedStatus) {
    Status status = Status::NotSupported("Feature not supported");
    ASSERT_FALSE(status.ok());
    ASSERT_FALSE(status.IsNotFound());
    ASSERT_FALSE(status.IsCorruption());
    ASSERT_FALSE(status.IsIOError());
    ASSERT_EQ(status.ToString(), "NotSupported: Feature not supported");
    
    // Test default message
    Status status2 = Status::NotSupported();
    ASSERT_EQ(status2.ToString(), "NotSupported: Not Supported");
}

TEST_F(StatusTest, InvalidArgumentStatus) {
    Status status = Status::InvalidArgument("Invalid argument");
    ASSERT_FALSE(status.ok());
    ASSERT_FALSE(status.IsNotFound());
    ASSERT_FALSE(status.IsCorruption());
    ASSERT_FALSE(status.IsIOError());
    ASSERT_EQ(status.ToString(), "InvalidArgument: Invalid argument");
    
    // Test default message
    Status status2 = Status::InvalidArgument();
    ASSERT_EQ(status2.ToString(), "InvalidArgument: Invalid Argument");
}

TEST_F(StatusTest, IOErrorStatus) {
    Status status = Status::IOError("IO error occurred");
    ASSERT_FALSE(status.ok());
    ASSERT_FALSE(status.IsNotFound());
    ASSERT_FALSE(status.IsCorruption());
    ASSERT_TRUE(status.IsIOError());
    ASSERT_EQ(status.ToString(), "IOError: IO error occurred");
    
    // Test default message
    Status status2 = Status::IOError();
    ASSERT_TRUE(status2.IsIOError());
    ASSERT_EQ(status2.ToString(), "IOError: IO Error");
}

// WriteBatch tests
class WriteBatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        options_.create_if_missing = true;
        Status status = DB::Open(options_, "/tmp/testdb_writebatch", &db_);
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

TEST_F(WriteBatchTest, Clear) {
    WriteBatch batch;
    batch.Put("key1", "value1");
    batch.Put("key2", "value2");
    batch.Delete("key3");
    
    ASSERT_EQ(batch.Count(), 3);
    
    batch.Clear();
    ASSERT_EQ(batch.Count(), 0);
    
    // After clear, batch should be empty
    Status status = db_->Write(write_options_, &batch);
    ASSERT_TRUE(status.ok());
    
    // Verify no data was written
    std::string value;
    status = db_->Get(read_options_, "key1", &value);
    ASSERT_TRUE(status.IsNotFound());
}

TEST_F(WriteBatchTest, Count) {
    WriteBatch batch;
    ASSERT_EQ(batch.Count(), 0);
    
    batch.Put("key1", "value1");
    ASSERT_EQ(batch.Count(), 1);
    
    batch.Put("key2", "value2");
    ASSERT_EQ(batch.Count(), 2);
    
    batch.Delete("key3");
    ASSERT_EQ(batch.Count(), 3);
    
    batch.Delete("key4");
    ASSERT_EQ(batch.Count(), 4);
}

TEST_F(WriteBatchTest, EmptyBatch) {
    WriteBatch batch;
    ASSERT_EQ(batch.Count(), 0);
    
    Status status = db_->Write(write_options_, &batch);
    ASSERT_TRUE(status.ok());
}

TEST_F(WriteBatchTest, BatchWithOnlyDeletes) {
    // First add some keys
    db_->Put(write_options_, "key1", "value1");
    db_->Put(write_options_, "key2", "value2");
    
    // Then delete them in a batch
    WriteBatch batch;
    batch.Delete("key1");
    batch.Delete("key2");
    batch.Delete("nonexistent");  // Delete non-existent key
    
    Status status = db_->Write(write_options_, &batch);
    ASSERT_TRUE(status.ok());
    
    // Verify keys are deleted
    std::string value;
    status = db_->Get(read_options_, "key1", &value);
    ASSERT_TRUE(status.IsNotFound());
    
    status = db_->Get(read_options_, "key2", &value);
    ASSERT_TRUE(status.IsNotFound());
}

// Iterator advanced tests
class IteratorAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        options_.create_if_missing = true;
        Status status = DB::Open(options_, "/tmp/testdb_iterator_advanced", &db_);
        ASSERT_TRUE(status.ok());
        
        // Add test data
        WriteOptions write_options;
        db_->Put(write_options, "a", "value_a");
        db_->Put(write_options, "b", "value_b");
        db_->Put(write_options, "c", "value_c");
        db_->Put(write_options, "d", "value_d");
        db_->Put(write_options, "e", "value_e");
    }

    void TearDown() override {
        delete db_;
    }

    Options options_;
    ReadOptions read_options_;
    DB* db_;
};

TEST_F(IteratorAdvancedTest, SeekToLast) {
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    it->SeekToLast();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "e");
    ASSERT_EQ(it->value(), "value_e");
    
    delete it;
}

TEST_F(IteratorAdvancedTest, Prev) {
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    // Start at last
    it->SeekToLast();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "e");
    
    // Move backwards
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "d");
    
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "c");
    
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "b");
    
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "a");
    
    // Try to go before first (should stay at first, still valid)
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "a");
    
    delete it;
}

TEST_F(IteratorAdvancedTest, Seek) {
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    // Seek to existing key
    it->Seek("c");
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "c");
    ASSERT_EQ(it->value(), "value_c");
    
    // Seek to non-existent key (should position at next key)
    it->Seek("b5");
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "c");
    
    // Seek to key before first
    it->Seek("0");
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "a");
    
    // Seek to key after last
    it->Seek("z");
    ASSERT_FALSE(it->Valid());
    
    delete it;
}

TEST_F(IteratorAdvancedTest, NextAtEnd) {
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    // Go to last
    it->SeekToLast();
    ASSERT_TRUE(it->Valid());
    
    // Try to go beyond last
    it->Next();
    ASSERT_FALSE(it->Valid());
    
    // Try Next again when invalid
    it->Next();
    ASSERT_FALSE(it->Valid());
    
    delete it;
}

TEST_F(IteratorAdvancedTest, PrevAtBeginning) {
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    // Go to first
    it->SeekToFirst();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "a");
    
    // Try to go before first (stays at first, still valid)
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "a");
    
    // Try Prev again (still at first)
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "a");
    
    delete it;
}

TEST_F(IteratorAdvancedTest, KeyValueWhenInvalid) {
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    // Iterator starts invalid for empty case, but we have data
    it->SeekToFirst();
    ASSERT_TRUE(it->Valid());
    
    // Go beyond end
    it->Seek("z");
    ASSERT_FALSE(it->Valid());
    
    // key() and value() should return empty strings when invalid
    ASSERT_EQ(it->key(), "");
    ASSERT_EQ(it->value(), "");
    
    delete it;
}

TEST_F(IteratorAdvancedTest, EmptyDatabaseIterator) {
    // Create a new empty database
    Options options;
    options.create_if_missing = true;
    DB* empty_db;
    Status status = DB::Open(options, "/tmp/testdb_empty", &empty_db);
    ASSERT_TRUE(status.ok());
    
    ReadOptions read_options;
    Iterator* it = empty_db->NewIterator(read_options);
    ASSERT_NE(it, nullptr);
    
    // Iterator should be invalid for empty database
    it->SeekToFirst();
    ASSERT_FALSE(it->Valid());
    
    it->SeekToLast();
    ASSERT_FALSE(it->Valid());
    
    it->Seek("any");
    ASSERT_FALSE(it->Valid());
    
    ASSERT_EQ(it->key(), "");
    ASSERT_EQ(it->value(), "");
    
    delete it;
    delete empty_db;
}

TEST_F(IteratorAdvancedTest, ReverseIteration) {
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    // Iterate backwards from last to first
    std::vector<std::string> keys;
    it->SeekToLast();
    int count = 0;
    const int max_count = 10;  // Safety limit to prevent infinite loop
    while (it->Valid() && count < max_count) {
        keys.push_back(it->key());
        std::string current_key = it->key();
        it->Prev();
        // If we're still at the same key (at beginning), break to avoid infinite loop
        if (it->Valid() && it->key() == current_key) {
            break;
        }
        count++;
    }
    
    ASSERT_EQ(keys.size(), 5);
    ASSERT_EQ(keys[0], "e");
    ASSERT_EQ(keys[1], "d");
    ASSERT_EQ(keys[2], "c");
    ASSERT_EQ(keys[3], "b");
    ASSERT_EQ(keys[4], "a");
    
    delete it;
}

TEST_F(IteratorAdvancedTest, SeekAndIterate) {
    Iterator* it = db_->NewIterator(read_options_);
    ASSERT_NE(it, nullptr);
    
    // Seek to middle and iterate forward
    it->Seek("c");
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "c");
    
    std::vector<std::string> keys;
    for (; it->Valid(); it->Next()) {
        keys.push_back(it->key());
    }
    
    ASSERT_EQ(keys.size(), 3);
    ASSERT_EQ(keys[0], "c");
    ASSERT_EQ(keys[1], "d");
    ASSERT_EQ(keys[2], "e");
    
    delete it;
}

// DestroyDB test
TEST_F(KVEngineTest, DestroyDB) {
    // Create a database
    DB* db;
    Status status = DB::Open(options_, "/tmp/testdb_destroy", &db);
    ASSERT_TRUE(status.ok());
    
    db->Put(write_options_, "key1", "value1");
    delete db;
    
    // DestroyDB should succeed (for in-memory implementation)
    status = DestroyDB("/tmp/testdb_destroy", options_);
    ASSERT_TRUE(status.ok());
    
    // After destroy, we can still open a new database
    status = DB::Open(options_, "/tmp/testdb_destroy", &db);
    ASSERT_TRUE(status.ok());
    delete db;
}

// Options tests
class OptionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(OptionsTest, DefaultOptions) {
    Options options;
    ASSERT_FALSE(options.create_if_missing);
    ASSERT_FALSE(options.error_if_exists);
    ASSERT_FALSE(options.paranoid_checks);
    ASSERT_EQ(options.info_log, nullptr);
}

TEST_F(OptionsTest, DefaultReadOptions) {
    ReadOptions read_options;
    ASSERT_FALSE(read_options.verify_checksums);
    ASSERT_TRUE(read_options.fill_cache);
    ASSERT_EQ(read_options.snapshot, nullptr);
}

TEST_F(OptionsTest, DefaultWriteOptions) {
    WriteOptions write_options;
    ASSERT_FALSE(write_options.sync);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
