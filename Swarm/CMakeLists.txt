# ============================================================================
# Swarm - RHI for HiveEngine
# ============================================================================

add_library(Swarm STATIC)
add_subdirectory(extern/diligentcore)

target_sources(Swarm
        PRIVATE
        src/swarm_module.cpp
)

target_include_directories(Swarm
        PUBLIC
        include
)

target_link_libraries(Swarm
        PUBLIC
        Hive
        Comb
        Wax
        Terra

        Diligent-GraphicsEngineVk-shared
        Diligent-BuildSettings
)

target_compile_features(Swarm
        PUBLIC
        cxx_std_20
)
