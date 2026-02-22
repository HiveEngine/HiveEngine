# ============================================================================
# Testbed - Application Test Executable
# ============================================================================

# ============================================================================
# Executable Configuration
# ============================================================================

add_executable(Testbed src/linux_main.cpp)

if(WIN32)
    target_sources(Testbed PRIVATE src/win32_main.cpp)
endif ()

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


target_link_libraries(Testbed
    PRIVATE
        Hive
        Comb
        Swarm
        Terra
)
