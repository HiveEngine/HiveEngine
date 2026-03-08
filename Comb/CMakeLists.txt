# ============================================================================
# Comb - Memory Allocators
# ============================================================================

# ============================================================================
# Library Configuration
# ============================================================================

add_library(Comb STATIC)

hive_configure_target(Comb)

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

option(COMB_ENABLE_CALLSTACKS "Enable callstack capture for Comb allocators (VERY SLOW! 100x overhead)" OFF)

if(HIVE_FEATURE_MEM_DEBUG)
    target_compile_definitions(Comb
        PUBLIC
            COMB_MEM_DEBUG=1
    )
    message(STATUS "Comb: Memory debugging ENABLED (slow! Use only for debugging sessions)")

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

file(GLOB_RECURSE COMB_SOURCES CONFIGURE_DEPENDS src/comb/*.cpp)

target_sources(Comb
    PRIVATE
        ${COMB_SOURCES}
)
