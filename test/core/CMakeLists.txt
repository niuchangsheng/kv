# Core module tests
add_executable(test_db
    test_db.cpp
)

target_link_libraries(test_db
    kv_lib
    GTest::gtest
    GTest::gtest_main
)
