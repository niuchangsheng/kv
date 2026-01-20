# Batch module tests
add_executable(test_write_batch
    test_write_batch.cpp
)

target_link_libraries(test_write_batch
    kv_lib
    GTest::gtest
    GTest::gtest_main
)
