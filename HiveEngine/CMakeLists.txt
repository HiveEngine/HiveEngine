set(CMAKE_CXX_STANDARD 20)

#include_directories(src)
#
#Dependencies
add_subdirectory(dep/entt)

add_subdirectory(dep/easy_profiler)
set(EASY_PROFILER_NO_GUI ON)
set(EASY_PROFILER_NO_SAMPLES ON)
add_compile_definitions(BUILD_WITH_EASY_PROFILER)

add_subdirectory(dep/raylib)

add_library(stb_image dep/stb_image/stb_image.cpp dep/stb_image/stb_image.h)

#Source file
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
add_compile_definitions(HIVE_BACKEND_RAYLIB)

#Platform Specific
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(HIVE_PLATFORM_WINDOWS)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(HIVE_PLATFORM_LINUX)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif ()

#Library
add_library(HiveEngine ${SOURCES})
target_link_libraries(HiveEngine EnTT easy_profiler stb_image raylib)
target_include_directories(HiveEngine PRIVATE src)

#Include Directory
target_include_directories(HiveEngine PRIVATE dep/raylib/src)
target_include_directories(HiveEngine PRIVATE dep/entt/src)
target_include_directories(HiveEngine PRIVATE dep/easy_profiler/easy_profiler_core)

#Precompiled header
target_precompile_headers(HiveEngine PRIVATE src/hvpch.h)

