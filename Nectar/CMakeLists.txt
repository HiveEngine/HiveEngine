# ============================================================================
# Nectar - Asset Pipeline for HiveEngine
# ============================================================================

add_library(Nectar STATIC)

file(GLOB_RECURSE NECTAR_SOURCES src/nectar/*.cpp)

target_sources(Nectar
    PRIVATE
        ${NECTAR_SOURCES}
)

target_include_directories(Nectar
    PUBLIC
        include
        extern/stb
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Hive/extern/tinyobj
        extern/cgltf
)

target_link_libraries(Nectar
    PUBLIC
        Hive
        Comb
        Wax
)

target_compile_features(Nectar
    PUBLIC
        cxx_std_20
)

# External libraries (LZ4, Zstd) — compiled as C++ since no C compiler in preset
set(NECTAR_EXTERN_SOURCES
    extern/lz4/lz4.c
    extern/zstd/zstd.c
)
set_source_files_properties(${NECTAR_EXTERN_SOURCES} PROPERTIES LANGUAGE CXX)
target_sources(Nectar PRIVATE ${NECTAR_EXTERN_SOURCES})
target_include_directories(Nectar PRIVATE extern/lz4 extern/zstd)