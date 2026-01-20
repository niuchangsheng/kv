# Iterator module tests
add_executable(test_iterator
    test_iterator.cpp
)

target_link_libraries(test_iterator
    kv_lib
    GTest::gtest
    GTest::gtest_main
)
