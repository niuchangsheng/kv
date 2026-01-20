# Common module tests
add_executable(test_status
    test_status.cpp
)

target_link_libraries(test_status
    kv_lib
    GTest::gtest
    GTest::gtest_main
)

add_executable(test_options
    test_options.cpp
)

target_link_libraries(test_options
    kv_lib
    GTest::gtest
    GTest::gtest_main
)
