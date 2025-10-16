option(swarm_use_vulkan OFF)

add_library(swarm STATIC)
target_include_directories(swarm PUBLIC include PRIVATE src)
target_precompile_headers(swarm PRIVATE include/swarm/precomp.h)

add_subdirectory(extern/glm)
target_link_libraries(swarm PUBLIC glm)



if(swarm_use_vulkan STREQUAL "ON")
    add_subdirectory(extern/vkboot)
    find_package(Vulkan REQUIRED)
    target_compile_definitions(swarm PUBLIC SWARM_USE_VULKAN)
    target_sources(swarm PRIVATE
            src/swarm/vulkan/instance_vulkan.cpp
            src/swarm/vulkan/device_vulkan.cpp
            src/swarm/vulkan/surface_vulkan.cpp
            src/swarm/vulkan/surfacefactory_vulkan.cpp
            src/swarm/vulkan/renderpass_vulkan.cpp
            src/swarm/vulkan/swapchain_vulkan.cpp
            src/swarm/vulkan/shader_vulkan.cpp
            src/swarm/vulkan/program_vulkan.cpp
            src/swarm/vulkan/commandpool_vulkan.cpp
            src/swarm/vulkan/texture_vulkan.cpp)

    target_link_libraries(swarm PUBLIC vk-bootstrap Vulkan::Vulkan)
endif ()

