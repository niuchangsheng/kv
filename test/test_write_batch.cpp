#include "kv_engine.h"
#include "write_batch.h"
#include <gtest/gtest.h>

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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
