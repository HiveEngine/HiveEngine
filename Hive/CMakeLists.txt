# ============================================================================
# Hive - Core Module
# ============================================================================

# ============================================================================
# External Dependencies
# ============================================================================

# Configure fmt WITHOUT exceptions (game engine mode)
set(FMT_EXCEPTIONS OFF CACHE BOOL "Disable exceptions in fmt" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Don't install fmt" FORCE)

add_subdirectory(extern/fmt EXCLUDE_FROM_ALL)

# Silence warnings from fmt (external library)
if(TARGET fmt)
    target_compile_options(fmt PRIVATE
        $<$<CXX_COMPILER_ID:Clang>:-w>
        $<$<CXX_COMPILER_ID:GNU>:-w>
        $<$<CXX_COMPILER_ID:MSVC>:/w>
    )
endif()

# Tracy profiler (on-demand: only active when viewer connects)
if(HIVE_FEATURE_PROFILER)
    include(FetchContent)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.13.1
        GIT_SHALLOW TRUE
    )
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tracy)

    # Silence warnings from Tracy
    if(TARGET TracyClient)
        target_compile_options(TracyClient PRIVATE
            $<$<CXX_COMPILER_ID:Clang>:-w>
            $<$<CXX_COMPILER_ID:GNU>:-w>
            $<$<CXX_COMPILER_ID:MSVC>:/w>
        )
    endif()
endif()

# ============================================================================
# Library Configuration
# ============================================================================

add_library(Hive STATIC)

target_include_directories(Hive
    PUBLIC
        include
    PRIVATE
        src
)

target_precompile_headers(Hive
    PRIVATE
        include/hive/precomp.h
)

target_link_libraries(Hive
    PUBLIC
        fmt::fmt
        $<$<BOOL:${HIVE_FEATURE_PROFILER}>:TracyClient>
)

target_sources(Hive
    PRIVATE
        src/hive/core/assert.cpp
        src/hive/core/log.cpp
        src/hive/core/module.cpp
        src/hive/core/moduleregistry.cpp
)
