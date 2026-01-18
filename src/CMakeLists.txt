# Create a library from the KV engine source files
add_library(kv_engine_lib
    kv_engine.cpp
    status.cpp
    options.cpp
    iterator.cpp
    write_batch.cpp
    db_iterator.cpp
)

# Define the header files for the library
target_sources(kv_engine_lib
    PUBLIC
        FILE_SET HEADERS
        FILES
            kv_engine.h
            status.h
            options.h
            iterator.h
            write_batch.h
            db_iterator.h
)

# Create the executable
add_executable(kv_engine_app
    main.cpp
)

# Link the library to the executable
target_link_libraries(kv_engine_app
    kv_engine_lib
)