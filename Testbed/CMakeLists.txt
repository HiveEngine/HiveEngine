# ============================================================================
# Testbed - Application Test Executable
# ============================================================================

if(WIN32)
    add_executable(Testbed src/win32_main.cpp)
elseif(UNIX)
    add_executable(Testbed src/linux_main.cpp)
endif()

hive_configure_target(Testbed)

target_include_directories(Testbed
    PRIVATE
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
        Antennae
)
