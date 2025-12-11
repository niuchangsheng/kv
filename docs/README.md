# Build & test

Create a build directory, configure with CMake, build, and run tests:

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Files:

- [src/main.cpp](src/main.cpp)
- [src/lib.h](src/lib.h)
- [src/lib.cpp](src/lib.cpp)
- [CMakeLists.txt](CMakeLists.txt)
