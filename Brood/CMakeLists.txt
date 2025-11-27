cmake_minimum_required(VERSION 3.30.5)

# Base test sources (always present)
set(LARVAE_TEST_SOURCES
    main.cpp
    ../Comb/tests/test_platform.cpp
)

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

# Create the benchmark executable
add_executable(larvae_benchmark
    main_benchmark.cpp
)

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
