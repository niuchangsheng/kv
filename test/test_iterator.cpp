#include "kv_engine.h"
#include <gtest/gtest.h>
#include <vector>

class IteratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        options_.create_if_missing = true;
        Status status = DB::Open(options_, "/tmp/testdb_iterator", &db_);
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

TEST_F(IteratorTest, BasicIterator) {
    // Add some data
    db_->Put(write_options_, "key1", "value1");
    db_->Put(write_options_, "key2", "value2");
    db_->Put(write_options_, "key3", "value3");
    
    // Test iterator
    Iterator* it = db_->NewIterator(read_options_);
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
}

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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
