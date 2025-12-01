# ============================================================================
# Brood - Test Runner
# ============================================================================

# ============================================================================
# Test Discovery
# ============================================================================

set(LARVAE_TEST_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../Comb/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Wax/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Queen/tests
)

# Glob all test_*.cpp files from specified directories (recursive)
set(LARVAE_TEST_SOURCES main.cpp)
foreach(TEST_DIR ${LARVAE_TEST_DIRS})
    file(GLOB_RECURSE DIR_TESTS ${TEST_DIR}/test_*.cpp)
    list(APPEND LARVAE_TEST_SOURCES ${DIR_TESTS})
endforeach()

# ============================================================================
# Test Executable
# ============================================================================

add_executable(larvae_runner ${LARVAE_TEST_SOURCES})

target_link_libraries(larvae_runner
    PRIVATE
        Larvae
        Hive
        Comb
        Wax
        Queen
)

target_include_directories(larvae_runner
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Comb/include
        ${CMAKE_SOURCE_DIR}/Wax/include
        ${CMAKE_SOURCE_DIR}/Queen/include
)

set_target_properties(larvae_runner
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ============================================================================
# CTest Integration
# ============================================================================

enable_testing()
add_test(NAME AllTests COMMAND larvae_runner)

# ============================================================================
# Benchmark Discovery
# ============================================================================

# Glob all benchmark_*.cpp files from specified directories (recursive)
set(LARVAE_BENCHMARK_SOURCES main_benchmark.cpp)
foreach(TEST_DIR ${LARVAE_TEST_DIRS})
    file(GLOB_RECURSE DIR_BENCHMARKS ${TEST_DIR}/benchmark_*.cpp)
    list(APPEND LARVAE_BENCHMARK_SOURCES ${DIR_BENCHMARKS})
endforeach()

# ============================================================================
# Benchmark Executable
# ============================================================================

add_executable(larvae_benchmark ${LARVAE_BENCHMARK_SOURCES})

target_link_libraries(larvae_benchmark
    PRIVATE
        Larvae
        Hive
        Comb
        Wax
        Queen
)

target_include_directories(larvae_benchmark
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Comb/include
        ${CMAKE_SOURCE_DIR}/Wax/include
        ${CMAKE_SOURCE_DIR}/Queen/include
)

set_target_properties(larvae_benchmark
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
