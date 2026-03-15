# ============================================================================
# Waggle - Engine Application Layer for HiveEngine
# ============================================================================

add_library(Waggle STATIC)

hive_configure_target(Waggle)

target_sources(Waggle
    PRIVATE
        src/waggle/app.cpp
        src/waggle/registermodule.cpp
        src/waggle/systems/transform_system.cpp
        src/waggle/engine_runner.cpp
        src/waggle/project/project_manager.cpp
        src/waggle/project/gameplay_module.cpp
        src/waggle/project/cmake_generator.cpp
        src/waggle/project/project_scaffolder.cpp
)

target_include_directories(Waggle
    PUBLIC
        include
)

target_precompile_headers(Waggle
    PRIVATE
        include/waggle/precomp.h
)

target_link_libraries(Waggle
    PUBLIC
        Queen
        Hive
        Nectar
)

target_link_libraries(Waggle PUBLIC Terra Antennae Swarm)

if(HIVE_ENGINE_MODE STREQUAL "Editor" AND HIVE_FEATURE_IMGUI AND HIVE_HAS_INTERACTIVE_GRAPHICS)
    target_link_libraries(Waggle PRIVATE Forge)
endif()
