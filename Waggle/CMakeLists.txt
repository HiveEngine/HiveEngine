# ============================================================================
# Waggle - Engine Application Layer for HiveEngine
# ============================================================================

add_library(Waggle STATIC)

target_sources(Waggle
    PRIVATE
        src/waggle/app.cpp
        src/waggle/systems/transform_system.cpp
        src/waggle/engine_runner.cpp
        src/waggle/project/project_manager.cpp
        src/waggle/project/gameplay_module.cpp
        src/waggle/project/cmake_generator.cpp
)

target_include_directories(Waggle
    PUBLIC
        include
)

target_link_libraries(Waggle
    PUBLIC
        Queen
        Hive
        Nectar
)

if(HIVE_FEATURE_VULKAN OR HIVE_FEATURE_D3D12)
    target_link_libraries(Waggle PUBLIC Swarm)
endif()

if(HIVE_FEATURE_GLFW)
    target_link_libraries(Waggle PUBLIC Terra Antennae)
endif()

if(HIVE_FEATURE_IMGUI)
    target_link_libraries(Waggle PRIVATE Forge)
endif()

target_compile_features(Waggle
    PUBLIC
        cxx_std_20
)
