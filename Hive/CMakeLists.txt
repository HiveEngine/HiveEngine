
add_library(hive STATIC)
target_include_directories(hive PUBLIC include PRIVATE src)
target_precompile_headers(hive PRIVATE include/hive/precomp.h)

target_sources(hive PRIVATE src/hive/core/log.cpp src/hive/core/module.cpp src/hive/core/moduleregistry.cpp)

