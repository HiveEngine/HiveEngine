# ============================================================================
# Nectar - Asset Pipeline for HiveEngine
# ============================================================================

add_library(Nectar STATIC)

hive_configure_target(Nectar)

file(GLOB_RECURSE NECTAR_SOURCES src/nectar/*.cpp)

target_sources(Nectar
    PRIVATE
        ${NECTAR_SOURCES}
)

target_include_directories(Nectar
    PUBLIC
        include
)

target_include_directories(Nectar
    SYSTEM PRIVATE
        extern/cgltf
        extern/lz4
        extern/stb
        extern/zstd
        ${CMAKE_SOURCE_DIR}/Hive/extern/tinyobj
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

if(MSVC)
    target_compile_definitions(Nectar PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# External libraries (LZ4, Zstd) — compiled as C++ since no C compiler in preset
set(NECTAR_EXTERN_SOURCES
    extern/lz4/lz4.c
    extern/zstd/zstd.c
)
set_source_files_properties(${NECTAR_EXTERN_SOURCES} PROPERTIES LANGUAGE CXX)
if(MSVC)
    set_source_files_properties(${NECTAR_EXTERN_SOURCES} PROPERTIES COMPILE_OPTIONS "/w")
else()
    set_source_files_properties(${NECTAR_EXTERN_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
endif()
target_sources(Nectar PRIVATE ${NECTAR_EXTERN_SOURCES})
