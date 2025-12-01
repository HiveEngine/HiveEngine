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
)

target_sources(Hive
    PRIVATE
        src/hive/core/assert.cpp
        src/hive/core/log.cpp
        src/hive/core/module.cpp
        src/hive/core/moduleregistry.cpp
)
