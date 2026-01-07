# Create a library from the KV engine source files
add_library(kv_engine_lib
    kv_engine.cpp
)

# Define the header files for the library
target_sources(kv_engine_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            kv_engine.h
)

# Create the executable
add_executable(kv_engine_app
    main.cpp
)

# Link the library to the executable
target_link_libraries(kv_engine_app
    kv_engine_lib
)