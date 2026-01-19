#include "kv_engine.h"
#include <cassert>
#include <iostream>
#include <map>
#include <chrono>
#include <thread>
#include <random>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::cout << "Testing silent data corruption (long-running test)..." << std::endl;
    std::cout << "This test runs continuously to detect data corruption over time." << std::endl;
    std::cout << std::endl;

    // Open database
    Options options;
    options.create_if_missing = true;
    DB* db;
    Status status = DB::Open(options, "/tmp/testdb_silent_corruption", &db);
    assert(status.ok());

    WriteOptions write_options;
    ReadOptions read_options;

    // Test configuration (can be overridden by environment variables)
    const char* duration_env = std::getenv("KV_TEST_DURATION_SECONDS");
    const int test_duration_seconds = duration_env ? std::atoi(duration_env) : 30;
    
    const char* keys_env = std::getenv("KV_TEST_NUM_KEYS");
    const int num_keys = keys_env ? std::atoi(keys_env) : 1000;
    
    const char* interval_env = std::getenv("KV_TEST_VERIFY_INTERVAL_MS");
    const int verification_interval_ms = interval_env ? std::atoi(interval_env) : 100;
    
    std::cout << "  Configuration:" << std::endl;
    std::cout << "    Keys: " << num_keys << std::endl;
    std::cout << "    Duration: " << test_duration_seconds << " seconds" << std::endl;
    std::cout << "    Verification interval: " << verification_interval_ms << " ms" << std::endl;
    std::cout << std::endl;

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
    std::uniform_int_distribution<> key_dist(0, num_keys - 1);
    std::uniform_int_distribution<> value_dist(1, 1000);

    auto start_time = std::chrono::steady_clock::now();
    auto last_verification = start_time;
    auto last_status_report = start_time;

    std::cout << "  Starting continuous read/write test..." << std::endl;

    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time).count();

        // Check if test duration has elapsed
        if (elapsed >= test_duration_seconds) {
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
            
            status = db->Put(write_options, key, value);
            if (!status.ok()) {
                std::cerr << "  ERROR: Write failed for key " << key 
                         << ": " << status.ToString() << std::endl;
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
            status = db->Get(read_options, key, &actual_value);
            total_reads++;

            if (status.IsNotFound()) {
                // Key might not exist yet, that's OK
                if (expected_data.find(key) != expected_data.end()) {
                    // But we expected it to exist!
                    std::cerr << "  ERROR: Key " << key 
                             << " was written but now not found!" << std::endl;
                    not_found_errors++;
                }
            } else if (!status.ok()) {
                std::cerr << "  ERROR: Read failed for key " << key 
                         << ": " << status.ToString() << std::endl;
                other_errors++;
            } else {
                // Verify data correctness
                auto it = expected_data.find(key);
                if (it != expected_data.end()) {
                    if (actual_value != it->second) {
                        std::cerr << "  CORRUPTION DETECTED!" << std::endl;
                        std::cerr << "    Key: " << key << std::endl;
                        std::cerr << "    Expected: " << it->second << std::endl;
                        std::cerr << "    Actual: " << actual_value << std::endl;
                        std::cerr << "    Write count: " << write_count[key] << std::endl;
                        corruption_errors++;
                    }
                }
            }
        }

        // Periodic verification: check all expected keys
        auto time_since_verification = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_verification).count();
        
        if (time_since_verification >= verification_interval_ms) {
            // Verify all keys in expected_data
            for (const auto& pair : expected_data) {
                std::string actual_value;
                status = db->Get(read_options, pair.first, &actual_value);
                
                if (status.IsNotFound()) {
                    std::cerr << "  CORRUPTION: Key " << pair.first 
                             << " was written but now not found!" << std::endl;
                    not_found_errors++;
                } else if (!status.ok()) {
                    std::cerr << "  ERROR: Read failed for key " << pair.first 
                             << ": " << status.ToString() << std::endl;
                    other_errors++;
                } else if (actual_value != pair.second) {
                    std::cerr << "  CORRUPTION DETECTED!" << std::endl;
                    std::cerr << "    Key: " << pair.first << std::endl;
                    std::cerr << "    Expected: " << pair.second << std::endl;
                    std::cerr << "    Actual: " << actual_value << std::endl;
                    std::cerr << "    Write count: " << write_count[pair.first] << std::endl;
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
    std::cout << std::endl;
    std::cout << "  Performing final comprehensive verification..." << std::endl;
    int final_verification_errors = 0;
    
    for (const auto& pair : expected_data) {
        std::string actual_value;
        status = db->Get(read_options, pair.first, &actual_value);
        
        if (status.IsNotFound()) {
            std::cerr << "  FINAL ERROR: Key " << pair.first 
                     << " was written but now not found!" << std::endl;
            final_verification_errors++;
        } else if (!status.ok()) {
            std::cerr << "  FINAL ERROR: Read failed for key " << pair.first 
                     << ": " << status.ToString() << std::endl;
            final_verification_errors++;
        } else if (actual_value != pair.second) {
            std::cerr << "  FINAL CORRUPTION DETECTED!" << std::endl;
            std::cerr << "    Key: " << pair.first << std::endl;
            std::cerr << "    Expected: " << pair.second << std::endl;
            std::cerr << "    Actual: " << actual_value << std::endl;
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
    if (corruption_errors > 0 || not_found_errors > 0 || other_errors > 0 || final_verification_errors > 0) {
        std::cerr << "  FAILED: Data corruption or errors detected!" << std::endl;
        std::cerr << "  Total errors: " 
                 << (corruption_errors + not_found_errors + other_errors + final_verification_errors) 
                 << std::endl;
        delete db;
        return 1;
    }

    std::cout << "  ✓ No data corruption detected during " << test_duration_seconds 
             << " seconds of continuous operation" << std::endl;
    std::cout << "  ✓ All " << expected_data.size() << " keys verified correct" << std::endl;
    std::cout << std::endl;
    std::cout << "Silent data corruption test passed!" << std::endl;

    delete db;
    return 0;
}
