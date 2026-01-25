# SSTable test module
add_executable(test_sstable
    test_sstable.cpp
)

target_link_libraries(test_sstable
    kv_lib
    GTest::gtest
    GTest::gtest_main
)
