# Core module: Main DB interface
add_library(core_lib
    db.cpp
)

target_sources(core_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            db.h
)

# Set include directory so other modules can use core/db.h
target_include_directories(core_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Core depends on common, iterator, batch, wal, memtable, and sstable
target_link_libraries(core_lib
    PUBLIC
        common_lib
        wal_lib
        iterator_lib
        batch_lib
        memtable_lib
        sstable_lib
)
