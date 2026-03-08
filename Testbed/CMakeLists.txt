# ============================================================================
# Testbed - Application Test Executable
# ============================================================================

# ============================================================================
# Executable Configuration
# ============================================================================

if(NOT (HIVE_FEATURE_GLFW AND (HIVE_FEATURE_VULKAN OR HIVE_FEATURE_D3D12)))
    return()
endif()

if(WIN32)
    add_executable(Testbed src/win32_main.cpp)
elseif(UNIX)
    add_executable(Testbed src/linux_main.cpp)
endif()

hive_configure_target(Testbed)

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
        Antennae
)
