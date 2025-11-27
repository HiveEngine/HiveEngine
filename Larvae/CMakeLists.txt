cmake_minimum_required(VERSION 3.30.5)

add_library(larvae STATIC)
target_include_directories(larvae PUBLIC include PRIVATE src)
target_precompile_headers(larvae PRIVATE include/larvae/precomp.h)

# Link against Hive (core module)
target_link_libraries(larvae PUBLIC hive)

# Add source files
target_sources(larvae PRIVATE
    src/larvae/test_registry.cpp
    src/larvae/test_runner.cpp
    src/larvae/assertions.cpp
    src/larvae/benchmark.cpp
    src/larvae/benchmark_registry.cpp
    src/larvae/benchmark_runner.cpp
    src/larvae/larvae.cpp
)
