cmake_minimum_required(VERSION 3.30.5)

add_library(comb STATIC)
target_include_directories(comb PUBLIC include PRIVATE src)
target_precompile_headers(comb PRIVATE include/comb/precomp.h)

# Link against Hive (core module)
target_link_libraries(comb PUBLIC hive)

# ============================================================================
# Memory Debugging Option
# ============================================================================

set(COMB_ENABLE_MEM_DEBUG ON CACHE BOOL "Enable comprehensive memory debugging (SLOW! 10-100x overhead)" FORCE)

if(COMB_ENABLE_MEM_DEBUG)
    target_compile_definitions(comb PUBLIC COMB_MEM_DEBUG=1)
    message(STATUS "Comb: Memory debugging ENABLED (slow! Use only for debugging sessions)")

    # Platform-specific callstack capture (optional, very expensive)
    option(COMB_ENABLE_CALLSTACKS "Enable callstack capture (VERY SLOW! 100x overhead)" OFF)
    if(COMB_ENABLE_CALLSTACKS)
        target_compile_definitions(comb PUBLIC COMB_MEM_DEBUG_CALLSTACKS=1)
        message(STATUS "Comb: Callstack capture ENABLED (extremely slow!)")

        # Windows: Link dbghelp.lib for symbol resolution
        if(WIN32)
            target_link_libraries(comb PRIVATE dbghelp)
        endif()
    else()
        target_compile_definitions(comb PUBLIC COMB_MEM_DEBUG_CALLSTACKS=0)
    endif()
else()
    target_compile_definitions(comb PUBLIC COMB_MEM_DEBUG=0)
    target_compile_definitions(comb PUBLIC COMB_MEM_DEBUG_CALLSTACKS=0)
    message(STATUS "Comb: Memory debugging disabled (zero overhead)")
endif()

# Glob all source files
file(GLOB_RECURSE COMB_SOURCES
    src/comb/*.cpp
)
target_sources(comb PRIVATE ${COMB_SOURCES})