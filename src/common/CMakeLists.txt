# Common module: Status and Options
add_library(common_lib
    status.cpp
    options.cpp
)

target_sources(common_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            status.h
            options.h
)

# Set include directory so other modules can use common/status.h
target_include_directories(common_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
