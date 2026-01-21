#include "wal/wal.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

class WALTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/kv_wal_test";
        wal_file_ = test_dir_ + "/LOG";
        
        // Clean up test directory
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
        fs::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Clean up test directory
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }
    
    std::string test_dir_;
    std::string wal_file_;
};

// Test WALWriter basic operations
TEST_F(WALTest, WriterBasic) {
    WALWriter writer(wal_file_);
    ASSERT_TRUE(writer.IsOpen());
    
    Status status = writer.AddRecord(kPut, "key1", "value1");
    ASSERT_TRUE(status.ok());
    
    status = writer.AddRecord(kDelete, "key2", "");
    ASSERT_TRUE(status.ok());
    
    status = writer.Sync();
    ASSERT_TRUE(status.ok());
    
    status = writer.Close();
    ASSERT_TRUE(status.ok());
}

// Test WALReader basic operations
TEST_F(WALTest, ReaderBasic) {
    // First write some records
    {
        WALWriter writer(wal_file_);
        ASSERT_TRUE(writer.IsOpen());
        writer.AddRecord(kPut, "key1", "value1");
        writer.AddRecord(kPut, "key2", "value2");
        writer.AddRecord(kDelete, "key1", "");
        writer.Close();
    }
    
    // Then read them back
    WALReader reader(wal_file_);
    ASSERT_TRUE(reader.IsOpen());
    
    RecordType type;
    std::string key, value;
    Status status = Status::OK();
    
    // Read first record
    ASSERT_TRUE(reader.ReadRecord(&type, &key, &value, &status));
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(type, kPut);
    ASSERT_EQ(key, "key1");
    ASSERT_EQ(value, "value1");
    
    // Read second record
    ASSERT_TRUE(reader.ReadRecord(&type, &key, &value, &status));
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(type, kPut);
    ASSERT_EQ(key, "key2");
    ASSERT_EQ(value, "value2");
    
    // Read third record
    ASSERT_TRUE(reader.ReadRecord(&type, &key, &value, &status));
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(type, kDelete);
    ASSERT_EQ(key, "key1");
    ASSERT_EQ(value, "");
    
    // Should be EOF now
    ASSERT_FALSE(reader.ReadRecord(&type, &key, &value, &status));
    ASSERT_TRUE(status.ok());
}

// Test WAL Replay
TEST_F(WALTest, Replay) {
    // Write some records
    {
        WALWriter writer(wal_file_);
        writer.AddRecord(kPut, "key1", "value1");
        writer.AddRecord(kPut, "key2", "value2");
        writer.AddRecord(kDelete, "key1", "");
        writer.Close();
    }
    
    // Replay handler
    class TestHandler : public WALReader::Handler {
    public:
        std::map<std::string, std::string> data;
        
        Status Put(const std::string& key, const std::string& value) override {
            data[key] = value;
            return Status::OK();
        }
        
        Status Delete(const std::string& key) override {
            data.erase(key);
            return Status::OK();
        }
    };
    
    TestHandler handler;
    WALReader reader(wal_file_);
    Status status = reader.Replay(&handler);
    
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(handler.data.size(), 1);
    ASSERT_EQ(handler.data["key2"], "value2");
    ASSERT_EQ(handler.data.find("key1"), handler.data.end());
}

// Test checksum verification
TEST_F(WALTest, ChecksumVerification) {
    // Write a record
    {
        WALWriter writer(wal_file_);
        writer.AddRecord(kPut, "key1", "value1");
        writer.Close();
    }
    
    // Corrupt the file by modifying a byte
    {
        std::fstream file(wal_file_, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(10);  // Skip header, modify key
        file.put('X');  // Corrupt a byte
        file.close();
    }
    
    // Try to read - should fail checksum verification
    WALReader reader(wal_file_);
    RecordType type;
    std::string key, value;
    Status status = Status::OK();
    
    // Should fail due to checksum mismatch
    bool read_ok = reader.ReadRecord(&type, &key, &value, &status);
    // Either read fails or checksum verification fails
    if (read_ok) {
        ASSERT_FALSE(status.ok());
        ASSERT_TRUE(status.IsCorruption());
    }
}

// Test empty key and value
TEST_F(WALTest, EmptyKeyValue) {
    WALWriter writer(wal_file_);
    writer.AddRecord(kPut, "", "value");
    writer.AddRecord(kPut, "key", "");
    writer.Close();
    
    WALReader reader(wal_file_);
    RecordType type;
    std::string key, value;
    Status status = Status::OK();
    
    ASSERT_TRUE(reader.ReadRecord(&type, &key, &value, &status));
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(key, "");
    ASSERT_EQ(value, "value");
    
    ASSERT_TRUE(reader.ReadRecord(&type, &key, &value, &status));
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(key, "key");
    ASSERT_EQ(value, "");
}
