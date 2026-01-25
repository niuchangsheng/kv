# SSTable module: Sorted String Table implementation
add_library(sstable_lib
    coding.h
    crc32.cpp
    block_builder.cpp
    block_reader.cpp
    sstable_builder.cpp
    sstable_reader.cpp
)

target_sources(sstable_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            coding.h
            crc32.h
            block_builder.h
            block_reader.h
            sstable_builder.h
            sstable_reader.h
)

# Set include directory so other modules can use sstable/*.h
target_include_directories(sstable_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# SSTable depends on common (for Status)
target_link_libraries(sstable_lib
    PUBLIC
        common_lib
)
