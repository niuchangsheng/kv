# MemTable module: In-memory sorted table
add_library(memtable_lib
    memtable.cpp
)

target_sources(memtable_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            memtable.h
)

# Set include directory so other modules can use memtable/memtable.h
target_include_directories(memtable_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# MemTable depends on common (for Status) and iterator (for Iterator interface)
target_link_libraries(memtable_lib
    PUBLIC
        common_lib
        iterator_lib
)
