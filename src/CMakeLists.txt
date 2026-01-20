# Create a library from the KV engine source files
add_library(kv_engine_lib
    db.cpp
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
            db.h
            status.h
            options.h
            iterator.h
            write_batch.h
            db_iterator.h
)