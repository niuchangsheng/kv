# WAL tests

add_executable(test_wal
    test_wal.cpp
)

target_link_libraries(test_wal
    kv_lib
    GTest::gtest
    GTest::gtest_main
)
