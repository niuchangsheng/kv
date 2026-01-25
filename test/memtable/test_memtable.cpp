#include "memtable/memtable.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>

class MemTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        memtable_ = std::make_unique<MemTable>();
    }

    void TearDown() override {
        memtable_.reset();
    }

    std::unique_ptr<MemTable> memtable_;
};

// Test basic Put and Get
TEST_F(MemTableTest, PutGet) {
    memtable_->Put("key1", "value1");
    memtable_->Put("key2", "value2");
    
    std::string value;
    ASSERT_TRUE(memtable_->Get("key1", &value));
    ASSERT_EQ(value, "value1");
    
    ASSERT_TRUE(memtable_->Get("key2", &value));
    ASSERT_EQ(value, "value2");
    
    ASSERT_FALSE(memtable_->Get("key3", &value));
}

// Test update
TEST_F(MemTableTest, Update) {
    memtable_->Put("key1", "value1");
    memtable_->Put("key1", "value2");
    
    std::string value;
    ASSERT_TRUE(memtable_->Get("key1", &value));
    ASSERT_EQ(value, "value2");
}

// Test Delete
TEST_F(MemTableTest, Delete) {
    memtable_->Put("key1", "value1");
    
    std::string value;
    ASSERT_TRUE(memtable_->Get("key1", &value));
    
    memtable_->Delete("key1");
    ASSERT_FALSE(memtable_->Get("key1", &value));
}

// Test Delete non-existent key
TEST_F(MemTableTest, DeleteNonExistent) {
    memtable_->Delete("nonexistent");
    
    std::string value;
    ASSERT_FALSE(memtable_->Get("nonexistent", &value));
}

// Test ApproximateSize
TEST_F(MemTableTest, ApproximateSize) {
    ASSERT_EQ(memtable_->ApproximateSize(), 0);
    
    memtable_->Put("key1", "value1");
    size_t size1 = memtable_->ApproximateSize();
    ASSERT_GT(size1, 0);
    
    memtable_->Put("key2", "value2");
    size_t size2 = memtable_->ApproximateSize();
    ASSERT_GT(size2, size1);
    
    // Update should update size
    memtable_->Put("key1", "longer_value");
    size_t size3 = memtable_->ApproximateSize();
    ASSERT_GT(size3, size2);
}

// Test Empty
TEST_F(MemTableTest, Empty) {
    ASSERT_TRUE(memtable_->Empty());
    
    memtable_->Put("key1", "value1");
    ASSERT_FALSE(memtable_->Empty());
}

// Test Size
TEST_F(MemTableTest, Size) {
    ASSERT_EQ(memtable_->Size(), 0);
    
    memtable_->Put("key1", "value1");
    ASSERT_EQ(memtable_->Size(), 1);
    
    memtable_->Put("key2", "value2");
    ASSERT_EQ(memtable_->Size(), 2);
    
    // Update doesn't change size
    memtable_->Put("key1", "value3");
    ASSERT_EQ(memtable_->Size(), 2);
    
    // Delete keeps entry (marks as deleted)
    memtable_->Delete("key1");
    ASSERT_EQ(memtable_->Size(), 2);
}

// Test Iterator
TEST_F(MemTableTest, Iterator) {
    memtable_->Put("a", "value_a");
    memtable_->Put("b", "value_b");
    memtable_->Put("c", "value_c");
    
    Iterator* it = memtable_->NewIterator();
    ASSERT_NE(it, nullptr);
    
    // Seek to first
    it->SeekToFirst();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "a");
    ASSERT_EQ(it->value(), "value_a");
    
    // Next
    it->Next();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "b");
    ASSERT_EQ(it->value(), "value_b");
    
    // Next
    it->Next();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "c");
    ASSERT_EQ(it->value(), "value_c");
    
    // Next (should be invalid)
    it->Next();
    ASSERT_FALSE(it->Valid());
    
    delete it;
}

// Test Iterator Seek
TEST_F(MemTableTest, IteratorSeek) {
    memtable_->Put("apple", "value1");
    memtable_->Put("banana", "value2");
    memtable_->Put("cherry", "value3");
    
    Iterator* it = memtable_->NewIterator();
    
    // Seek to "banana"
    it->Seek("banana");
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "banana");
    
    // Seek to "apricot" (should find "banana" as next)
    it->Seek("apricot");
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "banana");
    
    // Seek to "zebra" (should be invalid)
    it->Seek("zebra");
    ASSERT_FALSE(it->Valid());
    
    delete it;
}

// Test Iterator SeekToLast
TEST_F(MemTableTest, IteratorSeekToLast) {
    memtable_->Put("a", "value_a");
    memtable_->Put("b", "value_b");
    memtable_->Put("c", "value_c");
    
    Iterator* it = memtable_->NewIterator();
    it->SeekToLast();
    
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "c");
    ASSERT_EQ(it->value(), "value_c");
    
    // Prev
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "b");
    
    // Prev
    it->Prev();
    ASSERT_TRUE(it->Valid());
    ASSERT_EQ(it->key(), "a");
    
    // Prev again (should be invalid - already at first)
    it->Prev();
    ASSERT_FALSE(it->Valid());
    
    delete it;
}

// Test Iterator with deleted entries
TEST_F(MemTableTest, IteratorWithDeletes) {
    memtable_->Put("a", "value_a");
    memtable_->Put("b", "value_b");
    memtable_->Delete("b");
    memtable_->Put("c", "value_c");
    
    Iterator* it = memtable_->NewIterator();
    it->SeekToFirst();
    
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    for (; it->Valid(); it->Next()) {
        keys.push_back(it->key());
        values.push_back(it->value());
    }
    
    ASSERT_EQ(keys.size(), 3);
    ASSERT_EQ(keys[0], "a");
    ASSERT_EQ(keys[1], "b");
    ASSERT_EQ(keys[2], "c");
    
    ASSERT_EQ(values[0], "value_a");
    // Deleted entry has deletion marker (single null byte)
    ASSERT_EQ(values[1].size(), 1);
    ASSERT_EQ(values[1][0], '\0');
    ASSERT_EQ(values[2], "value_c");
    
    delete it;
}

// Test large number of entries
TEST_F(MemTableTest, LargeEntries) {
    const int num_entries = 1000;
    
    for (int i = 0; i < num_entries; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        memtable_->Put(key, value);
    }
    
    ASSERT_EQ(memtable_->Size(), num_entries);
    
    // Verify all entries
    for (int i = 0; i < num_entries; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expected_value = "value" + std::to_string(i);
        std::string value;
        
        ASSERT_TRUE(memtable_->Get(key, &value));
        ASSERT_EQ(value, expected_value);
    }
}

// Test ordered iteration
TEST_F(MemTableTest, OrderedIteration) {
    // Insert in random order
    memtable_->Put("z", "value_z");
    memtable_->Put("a", "value_a");
    memtable_->Put("m", "value_m");
    memtable_->Put("d", "value_d");
    
    Iterator* it = memtable_->NewIterator();
    it->SeekToFirst();
    
    std::vector<std::string> keys;
    for (; it->Valid(); it->Next()) {
        keys.push_back(it->key());
    }
    
    // Should be in sorted order
    ASSERT_EQ(keys.size(), 4);
    ASSERT_EQ(keys[0], "a");
    ASSERT_EQ(keys[1], "d");
    ASSERT_EQ(keys[2], "m");
    ASSERT_EQ(keys[3], "z");
    
    delete it;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
