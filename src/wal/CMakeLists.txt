# WAL (Write-Ahead Log) module

# Source files
set(WAL_SOURCES
    wal.cpp
)

# Create WAL library
add_library(wal_lib STATIC ${WAL_SOURCES})

# Include directories
target_include_directories(wal_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link dependencies
target_link_libraries(wal_lib
    PUBLIC
        common_lib
)
