# ============================================================================
# Swarm - RHI for HiveEngine
# ============================================================================

add_library(Swarm STATIC)
add_subdirectory(extern/diligentcore)

target_sources(Swarm
        PRIVATE
		src/swarm.cpp
        src/diligent_swarm.cpp
		src/diligent_swarm_vulkan.cpp

		src/diligent_swarm_linux_vulkan.cpp
		src/diligent_swarm_win32_vulkan.cpp
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

		Diligent-GraphicsEngineVk-static
		Diligent-GraphicsEngine
		Diligent-BuildSettings
)

target_compile_features(Swarm
        PUBLIC
        cxx_std_20
)
