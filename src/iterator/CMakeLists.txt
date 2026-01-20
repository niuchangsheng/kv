# Iterator module: Iterator interface and DBIterator implementation
add_library(iterator_lib
    iterator.cpp
    db_iterator.cpp
)

target_sources(iterator_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            iterator.h
            db_iterator.h
)

# Set include directory so other modules can use iterator/iterator.h
target_include_directories(iterator_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Iterator depends on common (for Status)
target_link_libraries(iterator_lib
    PUBLIC
        common_lib
)
