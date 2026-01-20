# Find Google Test
find_package(GTest REQUIRED)

# Create the test executable
add_executable(test_db
    test_db.cpp
)

# Link the KV engine library and GTest to the test
target_link_libraries(test_db
    kv_lib
    GTest::gtest
    GTest::gtest_main
)

# Create test_status executable
add_executable(test_status
    test_status.cpp
)

target_link_libraries(test_status
    kv_lib
    GTest::gtest
    GTest::gtest_main
)

# Create test_write_batch executable
add_executable(test_write_batch
    test_write_batch.cpp
)

target_link_libraries(test_write_batch
    kv_lib
    GTest::gtest
    GTest::gtest_main
)

# Create test_iterator executable
add_executable(test_iterator
    test_iterator.cpp
)

target_link_libraries(test_iterator
    kv_lib
    GTest::gtest
    GTest::gtest_main
)

# Create test_options executable
add_executable(test_options
    test_options.cpp
)

target_link_libraries(test_options
    kv_lib
    GTest::gtest
    GTest::gtest_main
)

# Create the benchmark executable
add_executable(kv_bench
    kv_bench.cpp
)

# Link the KV engine library to the benchmark
target_link_libraries(kv_bench
    kv_lib
)

# Create the long-running data correctness test executable
add_executable(kv_data_correctness
    kv_data_correctness.cpp
)

# Link the KV engine library and GTest to the data correctness test
target_link_libraries(kv_data_correctness
    kv_lib
    GTest::gtest
    GTest::gtest_main
)

# ============================================================================
# Register all tests (centralized test management)
# ============================================================================

add_test(
    NAME test_db
    COMMAND test_db
)

add_test(
    NAME test_status
    COMMAND test_status
)

add_test(
    NAME test_write_batch
    COMMAND test_write_batch
)

add_test(
    NAME test_iterator
    COMMAND test_iterator
)

add_test(
    NAME test_options
    COMMAND test_options
)

# Register the data correctness test (optional, as it's long-running)
# Uncomment to include in 'make test'
# add_test(
#     NAME kv_data_correctness
#     COMMAND kv_data_correctness
# )
