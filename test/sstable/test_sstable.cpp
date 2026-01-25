#include "sstable/sstable_builder.h"
#include "sstable/sstable_reader.h"
#include "sstable/block_builder.h"
#include "sstable/block_reader.h"
#include "common/status.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

class SSTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/kv_sstable_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    std::string test_dir_;
};

// Test BlockBuilder basic functionality
TEST_F(SSTableTest, BlockBuilderBasic) {
    sstable::BlockBuilder builder(16);  // restart_interval = 16

    builder.Add("apple", "red");
    builder.Add("banana", "yellow");
    builder.Add("cherry", "red");

    std::string block_data = builder.Finish();
    ASSERT_FALSE(block_data.empty());
}

// Test BlockReader basic functionality
TEST_F(SSTableTest, BlockReaderBasic) {
    sstable::BlockBuilder builder(16);
    
    std::vector<std::pair<std::string, std::string>> entries = {
        {"apple", "red"},
        {"banana", "yellow"},
        {"cherry", "red"}
    };

    for (const auto& entry : entries) {
        builder.Add(entry.first, entry.second);
    }

    std::string block_data = builder.Finish();
    sstable::BlockReader reader(block_data);

    ASSERT_TRUE(reader.IsValid());

    Status s = reader.SeekToFirst();
    ASSERT_TRUE(s.ok());

    for (const auto& entry : entries) {
        ASSERT_TRUE(reader.Valid());
        ASSERT_EQ(reader.key(), entry.first);
        ASSERT_EQ(reader.value(), entry.second);
        s = reader.Next();
        ASSERT_TRUE(s.ok() || s.IsNotFound());
    }
}

// Test shared prefix compression
TEST_F(SSTableTest, SharedPrefixCompression) {
    sstable::BlockBuilder builder(16);

    builder.Add("user:001", "value1");
    builder.Add("user:002", "value2");
    builder.Add("user:003", "value3");

    std::string block_data = builder.Finish();
    sstable::BlockReader reader(block_data);

    ASSERT_TRUE(reader.IsValid());
    Status s = reader.SeekToFirst();
    ASSERT_TRUE(s.ok());

    ASSERT_TRUE(reader.Valid());
    ASSERT_EQ(reader.key(), "user:001");
    ASSERT_EQ(reader.value(), "value1");

    s = reader.Next();
    ASSERT_TRUE(s.ok());
    ASSERT_EQ(reader.key(), "user:002");
    ASSERT_EQ(reader.value(), "value2");

    s = reader.Next();
    ASSERT_TRUE(s.ok());
    ASSERT_EQ(reader.key(), "user:003");
    ASSERT_EQ(reader.value(), "value3");
}

// Test restart points
TEST_F(SSTableTest, RestartPoints) {
    sstable::BlockBuilder builder(2);  // restart every 2 entries

    builder.Add("a", "1");
    builder.Add("b", "2");
    builder.Add("c", "3");
    builder.Add("d", "4");

    std::string block_data = builder.Finish();
    sstable::BlockReader reader(block_data);

    ASSERT_TRUE(reader.IsValid());
    ASSERT_GE(reader.NumRestarts(), 2);  // At least 2 restart points
}

// Test SSTableBuilder and SSTableReader
TEST_F(SSTableTest, SSTableBuildAndRead) {
    std::string sstable_file = test_dir_ + "/test.sst";

    // Build SSTable
    sstable::SSTableBuilder builder(sstable_file);

    std::vector<std::pair<std::string, std::string>> entries = {
        {"apple", "red"},
        {"banana", "yellow"},
        {"cherry", "red"},
        {"date", "brown"}
    };

    for (const auto& entry : entries) {
        Status s = builder.Add(entry.first, entry.second);
        ASSERT_TRUE(s.ok());
    }

    Status s = builder.Finish();
    ASSERT_TRUE(s.ok());
    ASSERT_EQ(builder.NumEntries(), 4);

    // Read SSTable
    sstable::SSTableReader reader(sstable_file);
    s = reader.Open();
    ASSERT_TRUE(s.ok());
    ASSERT_TRUE(reader.IsValid());

    // Test Get
    for (const auto& entry : entries) {
        std::string value;
        s = reader.Get(entry.first, &value);
        ASSERT_TRUE(s.ok());
        ASSERT_EQ(value, entry.second);
    }

    // Test non-existent key
    std::string value;
    s = reader.Get("nonexistent", &value);
    ASSERT_TRUE(s.IsNotFound());
}

// Test deletion markers
TEST_F(SSTableTest, DeletionMarkers) {
    std::string sstable_file = test_dir_ + "/test_delete.sst";

    sstable::SSTableBuilder builder(sstable_file);
    builder.Add("key1", "value1");
    builder.Add("key2", std::string(1, '\0'));  // Deletion marker
    builder.Add("key3", "value3");

    Status s = builder.Finish();
    ASSERT_TRUE(s.ok());

    sstable::SSTableReader reader(sstable_file);
    s = reader.Open();
    ASSERT_TRUE(s.ok());

    // key1 should exist
    std::string value;
    s = reader.Get("key1", &value);
    ASSERT_TRUE(s.ok());
    ASSERT_EQ(value, "value1");

    // key2 should be deleted
    s = reader.Get("key2", &value);
    ASSERT_TRUE(s.IsNotFound());

    // key3 should exist
    s = reader.Get("key3", &value);
    ASSERT_TRUE(s.ok());
    ASSERT_EQ(value, "value3");
}

// Test large number of entries
TEST_F(SSTableTest, LargeEntries) {
    std::string sstable_file = test_dir_ + "/test_large.sst";

    sstable::SSTableBuilder builder(sstable_file);

    const int num_entries = 1000;
    for (int i = 0; i < num_entries; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        Status s = builder.Add(key, value);
        ASSERT_TRUE(s.ok());
    }

    Status s = builder.Finish();
    ASSERT_TRUE(s.ok());
    ASSERT_EQ(builder.NumEntries(), num_entries);

    sstable::SSTableReader reader(sstable_file);
    s = reader.Open();
    ASSERT_TRUE(s.ok());

    // Verify all entries
    for (int i = 0; i < num_entries; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expected_value = "value" + std::to_string(i);
        std::string value;

        s = reader.Get(key, &value);
        ASSERT_TRUE(s.ok());
        ASSERT_EQ(value, expected_value);
    }
}

// Test BlockReader Seek
TEST_F(SSTableTest, BlockReaderSeek) {
    sstable::BlockBuilder builder(16);

    std::vector<std::string> keys = {"apple", "banana", "cherry", "date", "elderberry"};
    for (const auto& key : keys) {
        builder.Add(key, "value");
    }

    std::string block_data = builder.Finish();
    sstable::BlockReader reader(block_data);

    // Seek to "cherry"
    Status s = reader.Seek("cherry");
    ASSERT_TRUE(s.ok());
    ASSERT_TRUE(reader.Valid());
    ASSERT_EQ(reader.key(), "cherry");

    // Seek to "banana"
    s = reader.Seek("banana");
    ASSERT_TRUE(s.ok());
    ASSERT_TRUE(reader.Valid());
    ASSERT_EQ(reader.key(), "banana");

    // Seek to non-existent key (should find next)
    s = reader.Seek("coconut");
    ASSERT_TRUE(s.ok());
    ASSERT_TRUE(reader.Valid());
    ASSERT_EQ(reader.key(), "date");  // Next key after "coconut"
}
