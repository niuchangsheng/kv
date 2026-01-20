# Main CMakeLists.txt for test directory
# This file aggregates all test submodules

# Find Google Test
find_package(GTest REQUIRED)

# Add test subdirectories in dependency order
add_subdirectory(common)
add_subdirectory(iterator)
add_subdirectory(batch)
add_subdirectory(core)

# Benchmark executable (not a unit test)
add_executable(kv_bench
    kv_bench.cpp
)

target_link_libraries(kv_bench
    kv_lib
)

# Long-running data correctness test
add_executable(kv_data_correctness
    kv_data_correctness.cpp
)

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
