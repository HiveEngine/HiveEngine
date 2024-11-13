set(CMAKE_CXX_STANDARD 20)

#include_directories(src)
#
#Dependencies
if(BACKEND STREQUAL "raylib")
    message(STATUS "Building with Raylib backend")
    add_compile_definitions(HIVE_PLATFORM_RAYLIB)
    add_subdirectory(dep/raylib)
    include_directories(dep/raylib/src)
    add_library(backend ALIAS raylib)
    # Add your raylib-specific configurations here
else()
    message(STATUS "Building with GLFW")
    add_compile_definitions(HIVE_PLATFORM_GLFW)
    add_subdirectory(dep/glfw)
    include_directories(dep/glfw/include)
    add_library(backend ALIAS glfw)
    # Add your other backend configurations here
endif()

add_subdirectory(dep/glad)
include_directories(dep/glad/include)

add_subdirectory(dep/glm)
include_directories(dep/glm/glm)

add_subdirectory(dep/entt)

include_directories(dep/stb_image)
add_library(stb_image STATIC dep/stb_image/stb_image.cpp)



set(EASY_PROFILER_NO_GUI ON)
set(EASY_PROFILER_NO_SAMPLES ON)
add_compile_definitions(BUILD_WITH_EASY_PROFILER)

add_subdirectory(dep/raylib)

add_library(stb_image dep/stb_image/stb_image.cpp dep/stb_image/stb_image.h)

file(GLOB_RECURSE SCENE_SOURCES src/scene/*.cpp src/scene/*.h)
file(GLOB_RECURSE CORE_SOURCES src/core/*.cpp src/core/*.h)
file(GLOB_RECURSE PLATFORM_SOURCES src/platform/*.cpp src/platform/*.h)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(HIVE_PLATFORM_WINDOWS)
    add_compile_definitions(HIVE_RENDERING_OPENGL)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(HIVE_PLATFORM_LINUX)
    add_compile_definitions(HIVE_RENDERING_OPENGL)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif ()

add_library(HiveEngine ${CORE_SOURCES} ${PLATFORM_SOURCES} ${SCENE_SOURCES})
target_link_libraries(HiveEngine backend glad EnTT glm stb_image easy_profiler)
target_precompile_headers(HiveEngine PRIVATE ${PCH_HEADER})

#Include Directory
target_include_directories(HiveEngine PRIVATE dep/raylib/src)
target_include_directories(HiveEngine PRIVATE dep/entt/src)
target_include_directories(HiveEngine PRIVATE dep/easy_profiler/easy_profiler_core)

#Precompiled header
target_precompile_headers(HiveEngine PRIVATE src/hvpch.h)

