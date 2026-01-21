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

// Test WALWriter error cases
TEST_F(WALTest, WriterErrorCases) {
    // Test AddRecord when file is not open
    {
        WALWriter writer("/nonexistent/path/LOG");
        // File should fail to open
        if (!writer.IsOpen()) {
            Status status = writer.AddRecord(kPut, "key", "value");
            ASSERT_FALSE(status.ok());
            ASSERT_TRUE(status.IsIOError());
        }
    }
    
    // Test Sync when file is not open
    {
        WALWriter writer("/nonexistent/path/LOG");
        if (!writer.IsOpen()) {
            Status status = writer.Sync();
            ASSERT_FALSE(status.ok());
            ASSERT_TRUE(status.IsIOError());
        }
    }
    
    // Test Close when file is not open (should not fail)
    {
        WALWriter writer("/nonexistent/path/LOG");
        Status status = writer.Close();
        // Close should not fail even if file is not open
        ASSERT_TRUE(status.ok());
    }
}

// Test WALReader error cases
TEST_F(WALTest, ReaderErrorCases) {
    // Test ReadRecord when file is not open
    {
        WALReader reader("/nonexistent/path/LOG");
        if (!reader.IsOpen()) {
            RecordType type;
            std::string key, value;
            Status status = Status::OK();
            bool read_ok = reader.ReadRecord(&type, &key, &value, &status);
            ASSERT_FALSE(read_ok);
            ASSERT_FALSE(status.ok());
            ASSERT_TRUE(status.IsIOError());
        }
    }
    
    // Test Replay when file is not open
    {
        WALReader reader("/nonexistent/path/LOG");
        if (!reader.IsOpen()) {
            class TestHandler : public WALReader::Handler {
            public:
                Status Put(const std::string&, const std::string&) override {
                    return Status::OK();
                }
                Status Delete(const std::string&) override {
                    return Status::OK();
                }
            };
            TestHandler handler;
            Status status = reader.Replay(&handler);
            ASSERT_FALSE(status.ok());
            ASSERT_TRUE(status.IsIOError());
        }
    }
}

// Test invalid record type
TEST_F(WALTest, InvalidRecordType) {
    // Write a record with invalid type
    {
        std::ofstream file(wal_file_, std::ios::binary | std::ios::out);
        uint8_t invalid_type = 99;  // Invalid record type
        file.write(reinterpret_cast<const char*>(&invalid_type), 1);
        uint32_t key_len = 0;
        uint32_t value_len = 0;
        file.write(reinterpret_cast<const char*>(&key_len), 4);
        file.write(reinterpret_cast<const char*>(&value_len), 4);
        uint32_t checksum = 0;
        file.write(reinterpret_cast<const char*>(&checksum), 4);
        file.close();
    }
    
    // Try to read - should fail
    WALReader reader(wal_file_);
    RecordType type;
    std::string key, value;
    Status status = Status::OK();
    bool read_ok = reader.ReadRecord(&type, &key, &value, &status);
    ASSERT_FALSE(read_ok || status.ok());
    if (!status.ok()) {
        ASSERT_TRUE(status.IsCorruption());
    }
}

// Test corrupted key length
TEST_F(WALTest, CorruptedKeyLength) {
    // Write a record with corrupted key length (too large)
    {
        std::ofstream file(wal_file_, std::ios::binary | std::ios::out);
        uint8_t type = kPut;
        file.write(reinterpret_cast<const char*>(&type), 1);
        uint32_t key_len = 0xFFFFFFFF;  // Invalid: too large
        file.write(reinterpret_cast<const char*>(&key_len), 4);
        uint32_t value_len = 0;
        file.write(reinterpret_cast<const char*>(&value_len), 4);
        uint32_t checksum = 0;
        file.write(reinterpret_cast<const char*>(&checksum), 4);
        file.close();
    }
    
    // Try to read - should fail
    WALReader reader(wal_file_);
    RecordType type;
    std::string key, value;
    Status status = Status::OK();
    bool read_ok = reader.ReadRecord(&type, &key, &value, &status);
    ASSERT_FALSE(read_ok || status.ok());
}

// Test record extends beyond file
TEST_F(WALTest, RecordExtendsBeyondFile) {
    // Write a record with key length that extends beyond file
    {
        std::ofstream file(wal_file_, std::ios::binary | std::ios::out);
        uint8_t type = kPut;
        file.write(reinterpret_cast<const char*>(&type), 1);
        uint32_t key_len = 1000;  // But file only has header
        file.write(reinterpret_cast<const char*>(&key_len), 4);
        uint32_t value_len = 0;
        file.write(reinterpret_cast<const char*>(&value_len), 4);
        // Don't write key or checksum - file ends here
        file.close();
    }
    
    // Try to read - should fail
    WALReader reader(wal_file_);
    RecordType type;
    std::string key, value;
    Status status = Status::OK();
    bool read_ok = reader.ReadRecord(&type, &key, &value, &status);
    ASSERT_FALSE(read_ok || status.ok());
}

// Test Sync with sync option
TEST_F(WALTest, SyncWithSyncOption) {
    WALWriter writer(wal_file_);
    ASSERT_TRUE(writer.IsOpen());
    
    Status status = writer.AddRecord(kPut, "key1", "value1");
    ASSERT_TRUE(status.ok());
    
    // Test Sync
    status = writer.Sync();
    ASSERT_TRUE(status.ok());
    
    writer.Close();
}

// Test Replay with handler errors
TEST_F(WALTest, ReplayWithHandlerErrors) {
    // Write some records
    {
        WALWriter writer(wal_file_);
        writer.AddRecord(kPut, "key1", "value1");
        writer.Close();
    }
    
    // Handler that returns error
    class ErrorHandler : public WALReader::Handler {
    public:
        Status Put(const std::string&, const std::string&) override {
            return Status::IOError("Handler error");
        }
        Status Delete(const std::string&) override {
            return Status::IOError("Handler error");
        }
    };
    
    ErrorHandler handler;
    WALReader reader(wal_file_);
    Status status = reader.Replay(&handler);
    ASSERT_FALSE(status.ok());
    ASSERT_TRUE(status.IsIOError());
}
