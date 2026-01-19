# Create the test executable
add_executable(test_kv_engine
    test_kv_engine.cpp
)

# Link the KV engine library to the test
target_link_libraries(test_kv_engine
    kv_engine_lib
)

# Register the test
add_test(
    NAME test_kv_engine
    COMMAND test_kv_engine
)

# Create the benchmark executable
add_executable(kv_bench
    kv_bench.cpp
)

# Link the KV engine library to the benchmark
target_link_libraries(kv_bench
    kv_engine_lib
)

# Create the long-running data correctness test executable
add_executable(kv_data_correctness
    kv_data_correctness.cpp
)

# Link the KV engine library to the data correctness test
target_link_libraries(kv_data_correctness
    kv_engine_lib
)

# Register the data correctness test (optional, as it's long-running)
# Uncomment to include in 'make test'
# add_test(
#     NAME kv_data_correctness
#     COMMAND kv_data_correctness
# )