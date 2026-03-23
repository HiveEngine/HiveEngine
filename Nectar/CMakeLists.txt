# ============================================================================
# Nectar - Asset Pipeline for HiveEngine
# ============================================================================

add_library(Nectar ${HIVE_LIB_TYPE})

hive_configure_target(Nectar)

hive_get_vendor_warning_compile_options(NECTAR_VENDOR_WARNING_COMPILE_OPTIONS)

file(GLOB_RECURSE NECTAR_SOURCES CONFIGURE_DEPENDS src/nectar/*.cpp)
list(FILTER NECTAR_SOURCES EXCLUDE REGEX "native_file_watcher_(win32|linux)\\.cpp$")

if(WIN32)
    list(APPEND NECTAR_SOURCES src/nectar/watcher/native_file_watcher_win32.cpp)
elseif(UNIX)
    list(APPEND NECTAR_SOURCES src/nectar/watcher/native_file_watcher_linux.cpp)
endif()

target_sources(Nectar
    PRIVATE
        ${NECTAR_SOURCES}
)

target_include_directories(Nectar
    PUBLIC
        include
)

target_precompile_headers(Nectar
    PRIVATE
        include/nectar/precomp.h
)

target_include_directories(Nectar
    SYSTEM PRIVATE
        extern/cgltf
        extern/lz4
        extern/stb
        extern/zstd
        ${CMAKE_CURRENT_SOURCE_DIR}/../Hive/extern/tinyobj
)

target_link_libraries(Nectar
    PUBLIC
        Hive
        Comb
        Wax
        Drone
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
set_source_files_properties(${NECTAR_EXTERN_SOURCES} PROPERTIES COMPILE_OPTIONS "${NECTAR_VENDOR_WARNING_COMPILE_OPTIONS}")
target_sources(Nectar PRIVATE ${NECTAR_EXTERN_SOURCES})
