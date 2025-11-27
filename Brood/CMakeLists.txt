cmake_minimum_required(VERSION 3.30.5)

# Test directories to glob
set(LARVAE_TEST_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../Comb/tests
)

# Glob all test_*.cpp files from specified directories
set(LARVAE_TEST_SOURCES main.cpp)
foreach(TEST_DIR ${LARVAE_TEST_DIRS})
    file(GLOB DIR_TESTS ${TEST_DIR}/test_*.cpp)
    list(APPEND LARVAE_TEST_SOURCES ${DIR_TESTS})
endforeach()

# Create the test executable
add_executable(larvae_runner ${LARVAE_TEST_SOURCES})

# Link against Larvae testing framework and modules to test
target_link_libraries(larvae_runner PRIVATE
    larvae
    hive
    comb
)

target_include_directories(larvae_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/Comb/include
)

# Set output directory
set_target_properties(larvae_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add test to CTest
enable_testing()
add_test(NAME AllTests COMMAND larvae_runner)

# Glob all benchmark_*.cpp files from specified directories
set(LARVAE_BENCHMARK_SOURCES main_benchmark.cpp)
foreach(TEST_DIR ${LARVAE_TEST_DIRS})
    file(GLOB DIR_BENCHMARKS ${TEST_DIR}/benchmark_*.cpp)
    list(APPEND LARVAE_BENCHMARK_SOURCES ${DIR_BENCHMARKS})
endforeach()

# Create the benchmark executable
add_executable(larvae_benchmark ${LARVAE_BENCHMARK_SOURCES})

target_link_libraries(larvae_benchmark PRIVATE
    larvae
    hive
    comb
)

target_include_directories(larvae_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/Comb/include
)

set_target_properties(larvae_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
