add_executable(test_add test_add.cpp)
target_link_libraries(test_add PRIVATE kv_lib)
add_test(NAME test_add COMMAND test_add)
