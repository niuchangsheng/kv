add_library(kv_lib STATIC lib.cpp)
target_include_directories(kv_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(kv_app main.cpp)
target_link_libraries(kv_app PRIVATE kv_lib)
