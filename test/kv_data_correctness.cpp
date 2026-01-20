#include "core/db.h"
#include <gtest/gtest.h>
#include <map>
#include <chrono>
#include <thread>
#include <random>
#include <cstdlib>

class LongRunningDataCorrectnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        options_.create_if_missing = true;
        Status status = DB::Open(options_, "/tmp/testdb_silent_corruption", &db_);
        ASSERT_TRUE(status.ok());
        
        // Test configuration (can be overridden by environment variables)
        const char* duration_env = std::getenv("KV_TEST_DURATION_SECONDS");
        test_duration_seconds_ = duration_env ? std::atoi(duration_env) : 30;
        
        const char* keys_env = std::getenv("KV_TEST_NUM_KEYS");
        num_keys_ = keys_env ? std::atoi(keys_env) : 1000;
        
        const char* interval_env = std::getenv("KV_TEST_VERIFY_INTERVAL_MS");
        verification_interval_ms_ = interval_env ? std::atoi(interval_env) : 100;
    }

    void TearDown() override {
        delete db_;
    }

    Options options_;
    WriteOptions write_options_;
    ReadOptions read_options_;
    DB* db_;
    int test_duration_seconds_;
    int num_keys_;
    int verification_interval_ms_;
};

TEST_F(LongRunningDataCorrectnessTest, SilentDataCorruption) {
    // This test runs continuously to detect data corruption over time
    
    // Data structure to track expected values
    std::map<std::string, std::string> expected_data;
    std::map<std::string, int> write_count;  // Track how many times each key was written
    
    // Statistics
    int total_writes = 0;
    int total_reads = 0;
    int total_verifications = 0;
    int corruption_errors = 0;
    int not_found_errors = 0;
    int other_errors = 0;

    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(0, num_keys_ - 1);
    std::uniform_int_distribution<> value_dist(1, 1000);

    auto start_time = std::chrono::steady_clock::now();
    auto last_verification = start_time;
    auto last_status_report = start_time;

    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time).count();

        // Check if test duration has elapsed
        if (elapsed >= test_duration_seconds_) {
            break;
        }

        // Random operation: 70% write, 30% read
        int operation = value_dist(gen) % 10;
        
        if (operation < 7) {
            // Write operation
            int key_index = key_dist(gen);
            std::string key = "key_" + std::to_string(key_index);
            std::string value = "value_" + std::to_string(value_dist(gen)) + "_" + 
                               std::to_string(total_writes);
            
            Status status = db_->Put(write_options_, key, value);
            if (!status.ok()) {
                ADD_FAILURE() << "Write failed for key " << key 
                             << ": " << status.ToString();
                other_errors++;
                continue;
            }

            // Update expected data
            expected_data[key] = value;
            write_count[key]++;
            total_writes++;
        } else {
            // Read operation
            int key_index = key_dist(gen);
            std::string key = "key_" + std::to_string(key_index);
            
            std::string actual_value;
            Status status = db_->Get(read_options_, key, &actual_value);
            total_reads++;

            if (status.IsNotFound()) {
                // Key might not exist yet, that's OK
                if (expected_data.find(key) != expected_data.end()) {
                    // But we expected it to exist!
                    ADD_FAILURE() << "Key " << key 
                                 << " was written but now not found!";
                    not_found_errors++;
                }
            } else if (!status.ok()) {
                ADD_FAILURE() << "Read failed for key " << key 
                             << ": " << status.ToString();
                other_errors++;
            } else {
                // Verify data correctness
                auto it = expected_data.find(key);
                if (it != expected_data.end()) {
                    if (actual_value != it->second) {
                        ADD_FAILURE() << "CORRUPTION DETECTED! Key: " << key
                                     << ", Expected: " << it->second
                                     << ", Actual: " << actual_value
                                     << ", Write count: " << write_count[key];
                        corruption_errors++;
                    }
                }
            }
        }

        // Periodic verification: check all expected keys
        auto time_since_verification = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_verification).count();
        
        if (time_since_verification >= verification_interval_ms_) {
            // Verify all keys in expected_data
            for (const auto& pair : expected_data) {
                std::string actual_value;
                Status status = db_->Get(read_options_, pair.first, &actual_value);
                
                if (status.IsNotFound()) {
                    ADD_FAILURE() << "CORRUPTION: Key " << pair.first 
                                 << " was written but now not found!";
                    not_found_errors++;
                } else if (!status.ok()) {
                    ADD_FAILURE() << "ERROR: Read failed for key " << pair.first 
                                 << ": " << status.ToString();
                    other_errors++;
                } else if (actual_value != pair.second) {
                    ADD_FAILURE() << "CORRUPTION DETECTED! Key: " << pair.first
                                 << ", Expected: " << pair.second
                                 << ", Actual: " << actual_value
                                 << ", Write count: " << write_count[pair.first];
                    corruption_errors++;
                }
            }
            total_verifications++;
            last_verification = current_time;
        }

        // Periodic status report
        auto time_since_report = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_status_report).count();
        
        if (time_since_report >= 5) {
            std::cout << "  [" << elapsed << "s] Writes: " << total_writes 
                     << ", Reads: " << total_reads 
                     << ", Verifications: " << total_verifications
                     << ", Errors: " << (corruption_errors + not_found_errors + other_errors)
                     << std::endl;
            last_status_report = current_time;
        }

        // Small delay to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Final comprehensive verification
    int final_verification_errors = 0;
    
    for (const auto& pair : expected_data) {
        std::string actual_value;
        Status status = db_->Get(read_options_, pair.first, &actual_value);
        
        if (status.IsNotFound()) {
            ADD_FAILURE() << "FINAL ERROR: Key " << pair.first 
                         << " was written but now not found!";
            final_verification_errors++;
        } else if (!status.ok()) {
            ADD_FAILURE() << "FINAL ERROR: Read failed for key " << pair.first 
                         << ": " << status.ToString();
            final_verification_errors++;
        } else if (actual_value != pair.second) {
            ADD_FAILURE() << "FINAL CORRUPTION DETECTED! Key: " << pair.first
                         << ", Expected: " << pair.second
                         << ", Actual: " << actual_value;
            final_verification_errors++;
        }
    }

    // Print statistics
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();

    std::cout << std::endl;
    std::cout << "  Test Statistics:" << std::endl;
    std::cout << "    Total time: " << total_time << " ms" << std::endl;
    std::cout << "    Total writes: " << total_writes << std::endl;
    std::cout << "    Total reads: " << total_reads << std::endl;
    std::cout << "    Total verifications: " << total_verifications << std::endl;
    std::cout << "    Keys tracked: " << expected_data.size() << std::endl;
    std::cout << std::endl;
    std::cout << "  Error Statistics:" << std::endl;
    std::cout << "    Data corruption errors: " << corruption_errors << std::endl;
    std::cout << "    Not found errors: " << not_found_errors << std::endl;
    std::cout << "    Other errors: " << other_errors << std::endl;
    std::cout << "    Final verification errors: " << final_verification_errors << std::endl;
    std::cout << std::endl;

    // Assert no errors
    EXPECT_EQ(corruption_errors, 0) << "Data corruption errors detected!";
    EXPECT_EQ(not_found_errors, 0) << "Not found errors detected!";
    EXPECT_EQ(other_errors, 0) << "Other errors detected!";
    EXPECT_EQ(final_verification_errors, 0) << "Final verification errors detected!";
    
    std::cout << "  ✓ No data corruption detected during " << test_duration_seconds_ 
             << " seconds of continuous operation" << std::endl;
    std::cout << "  ✓ All " << expected_data.size() << " keys verified correct" << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
