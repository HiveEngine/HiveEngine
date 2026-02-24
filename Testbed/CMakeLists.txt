# ============================================================================
# Testbed - Application Test Executable
# ============================================================================

# ============================================================================
# Executable Configuration
# ============================================================================

if(WIN32)
    add_executable(Testbed src/win32_main.cpp)
elseif(UNIX)
    add_executable(Testbed src/linux_main.cpp)
endif()

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
