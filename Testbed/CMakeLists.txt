add_executable(testbed src/main.cpp src/systemmodule.cpp src/logtestbed.cpp)
target_include_directories(testbed PUBLIC include)
target_precompile_headers(testbed PRIVATE include/testbed/precomp.h)

target_link_libraries(testbed PRIVATE hive terra swarm)
