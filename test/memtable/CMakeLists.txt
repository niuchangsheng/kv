# MemTable test module
add_executable(test_memtable
    test_memtable.cpp
)

target_link_libraries(test_memtable
    kv_lib
    GTest::gtest
    GTest::gtest_main
)
