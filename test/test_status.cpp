#include "status.h"
#include <gtest/gtest.h>

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

TEST_F(StatusTest, UnknownStatusDefaultCase) {
    // Test the default case in ToString() switch statement
    // Since Status constructor and enum are private, we need to use
    // a workaround to create a Status with an unknown code.
    // We'll use pointer manipulation to access private members.
    
    // Create a valid Status first
    Status status = Status::IOError("Test message");
    
    // Access the private code_ member using pointer arithmetic
    // Status has two members: Code code_ and std::string msg_
    // We need to modify code_ to an invalid value
    struct StatusLayout {
        int code_;  // Assuming Code is an int (enum)
        std::string msg_;
    };
    
    // Use reinterpret_cast to access private members
    StatusLayout* layout = reinterpret_cast<StatusLayout*>(&status);
    
    // Set code_ to an invalid value (6, which is not in the enum)
    // This will trigger the default case in ToString()
    layout->code_ = 6;
    
    // Now ToString() should hit the default case
    std::string result = status.ToString();
    ASSERT_EQ(result, "Unknown: Test message");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
