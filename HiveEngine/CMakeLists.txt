set(CMAKE_CXX_STANDARD 20)
set(PCH_HEADER src/hvpch.h)

#Hive
file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(hive STATIC ${SOURCES})
target_include_directories(hive PUBLIC src)
target_precompile_headers(hive PUBLIC src/hvpch.h)

#glfw
add_subdirectory(dep/glfw)
target_link_libraries(hive PRIVATE glfw)
target_include_directories(hive PUBLIC dep/glfw/include)

#glm
add_subdirectory(dep/glm)
target_link_libraries(hive PRIVATE glm-header-only)
target_include_directories(hive PUBLIC dep/glm)

#stb
target_include_directories(hive PRIVATE dep/stb_image)

#tinyobjloader
target_include_directories(hive PRIVATE dep/tinyobjloader)

#Vulkan
if(WIN32)
    set(DEFAULT_VULKAN_SDK_PATH "C:/VulkanSDK/1.x.x.x")
    set(VULKAN_LIB_NAME vulkan-1)
elseif(UNIX)
    set(DEFAULT_VULKAN_SDK_PATH "")
    set(VULKAN_LIB_NAME vulkan)

#    Linux should have a vulkan package that install it in the correct location
else()
    message(FATAL_ERROR "Unsupported platform for Vulkan SDK.")
endif()

option(VULKAN_SDK_PATH "Path to Vulkan SDK" ${DEFAULT_VULKAN_SDK_PATH})
target_include_directories(hive PUBLIC ${VULKAN_SDK_PATH}/Include)
target_link_directories(hive PUBLIC ${VULKAN_SDK_PATH}/Lib)
target_link_libraries(hive PUBLIC ${VULKAN_LIB_NAME})
