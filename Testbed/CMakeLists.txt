# ============================================================================
# Testbed - Application Test Executable
# ============================================================================

# ============================================================================
# Executable Configuration
# ============================================================================

add_executable(Testbed
    src/main.cpp
    src/systemmodule.cpp
    src/logtestbed.cpp
)

target_include_directories(Testbed
    PUBLIC
        include
)

target_precompile_headers(Testbed
    PRIVATE
        include/testbed/precomp.h
)

# ============================================================================
# Dependencies
# ============================================================================

add_subdirectory(extern/swarm)

target_link_libraries(Testbed
    PRIVATE
        Hive
        Terra
        Swarm
)
