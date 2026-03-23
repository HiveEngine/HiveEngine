# ============================================================================
# Larvae - Testing Framework
# ============================================================================

# ============================================================================
# Library Configuration
# ============================================================================

add_library(Larvae STATIC)

hive_configure_target(Larvae)

target_include_directories(Larvae
    PUBLIC
        include
    PRIVATE
        src
)

target_precompile_headers(Larvae
    PRIVATE
        include/larvae/precomp.h
)

if(HIVE_BUILD_SHARED)
    target_link_libraries(Larvae PUBLIC hive_engine)
else()
    target_link_libraries(Larvae PUBLIC Hive)
endif()

target_sources(Larvae
    PRIVATE
        src/larvae/test_registry.cpp
        src/larvae/test_runner.cpp
        src/larvae/capabilities.cpp
        src/larvae/assertions.cpp
        src/larvae/benchmark.cpp
        src/larvae/benchmark_registry.cpp
        src/larvae/benchmark_runner.cpp
        src/larvae/larvae.cpp
)
