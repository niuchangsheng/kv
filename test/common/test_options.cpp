#include "common/options.h"
#include <gtest/gtest.h>

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
