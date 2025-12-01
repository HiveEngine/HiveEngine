# ============================================================================
# Comb - Memory Allocators
# ============================================================================

# ============================================================================
# Library Configuration
# ============================================================================

add_library(Comb STATIC)

target_include_directories(Comb
    PUBLIC
        include
    PRIVATE
        src
)

target_precompile_headers(Comb
    PRIVATE
        include/comb/precomp.h
)

target_link_libraries(Comb
    PUBLIC
        Hive
)

# ============================================================================
# Memory Debugging Options
# ============================================================================

# Auto-enable memory debugging for Debug/MemDebug builds, disable for Release
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "MemDebug")
    set(COMB_ENABLE_MEM_DEBUG_DEFAULT ON)
else()
    set(COMB_ENABLE_MEM_DEBUG_DEFAULT OFF)
endif()

option(COMB_ENABLE_MEM_DEBUG "Enable comprehensive memory debugging (SLOW! 10-100x overhead)" ${COMB_ENABLE_MEM_DEBUG_DEFAULT})

if(COMB_ENABLE_MEM_DEBUG)
    target_compile_definitions(Comb
        PUBLIC
            COMB_MEM_DEBUG=1
    )
    message(STATUS "Comb: Memory debugging ENABLED (slow! Use only for debugging sessions)")

    # Platform-specific callstack capture (optional, very expensive)
    option(COMB_ENABLE_CALLSTACKS "Enable callstack capture (VERY SLOW! 100x overhead)" OFF)

    if(COMB_ENABLE_CALLSTACKS)
        target_compile_definitions(Comb
            PUBLIC
                COMB_MEM_DEBUG_CALLSTACKS=1
        )
        message(STATUS "Comb: Callstack capture ENABLED (extremely slow!)")

        # Windows: Link dbghelp.lib for symbol resolution
        if(WIN32)
            target_link_libraries(Comb
                PRIVATE
                    dbghelp
            )
        endif()
    else()
        target_compile_definitions(Comb
            PUBLIC
                COMB_MEM_DEBUG_CALLSTACKS=0
        )
    endif()
else()
    target_compile_definitions(Comb
        PUBLIC
            COMB_MEM_DEBUG=0
            COMB_MEM_DEBUG_CALLSTACKS=0
    )
    message(STATUS "Comb: Memory debugging disabled (zero overhead)")
endif()

# ============================================================================
# Sources
# ============================================================================

file(GLOB_RECURSE COMB_SOURCES src/comb/*.cpp)

target_sources(Comb
    PRIVATE
        ${COMB_SOURCES}
)
