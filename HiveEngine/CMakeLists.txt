set(CMAKE_CXX_STANDARD 20)
set(PCH_HEADER src/hvpch.h)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(hive STATIC ${SOURCES})
target_include_directories(hive PUBLIC src)
target_precompile_headers(hive PUBLIC src/hvpch.h)

