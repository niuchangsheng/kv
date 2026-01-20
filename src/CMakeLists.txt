# Main CMakeLists.txt for src directory
# This file aggregates all submodules

# Add subdirectories in dependency order
add_subdirectory(common)
add_subdirectory(iterator)
add_subdirectory(batch)
add_subdirectory(core)

# Create the main library that aggregates all modules
# This is the library that external code (tests, benchmarks) will link against
add_library(kv_engine_lib INTERFACE)

# Link all submodules to the main library
target_link_libraries(kv_engine_lib
    INTERFACE
        core_lib
)

# The include directories are already set by the submodules,
# so external code can use:
#   #include "core/db.h"
#   #include "common/status.h"
#   #include "iterator/iterator.h"
#   #include "batch/write_batch.h"
