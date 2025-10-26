cmake_minimum_required(VERSION 3.30.5)
project(HiveEngine_Redo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(swarm_use_vulkan ON)
set(terra_use_glfw ON)

add_subdirectory(Hive)
#add_subdirectory(Swarm)
add_subdirectory(Terra)
add_subdirectory(Testbed)
