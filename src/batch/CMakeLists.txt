# Batch module: WriteBatch for atomic batch operations
add_library(batch_lib
    write_batch.cpp
)

target_sources(batch_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            write_batch.h
)

# Set include directory so other modules can use batch/write_batch.h
target_include_directories(batch_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Batch depends on common (for Status)
target_link_libraries(batch_lib
    PUBLIC
        common_lib
)
