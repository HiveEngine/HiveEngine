# ============================================================================
# Terra - Windowing Module
# ============================================================================

# ============================================================================
# Options
# ============================================================================

option(terra_use_glfw OFF)

# ============================================================================
# Library Configuration
# ============================================================================

add_library(Terra STATIC)

target_include_directories(Terra
    PUBLIC
        include
    PRIVATE
        src
)

target_precompile_headers(Terra
    PRIVATE
        include/terra/precomp.h
)

# ============================================================================
# GLFW Backend
# ============================================================================

if(terra_use_glfw STREQUAL "ON")
    add_subdirectory(extern/glfw)

    target_link_libraries(Terra
        PUBLIC
            glfw
    )

    target_compile_definitions(Terra
        PUBLIC
            TERRA_USE_GLFW
    )

    target_sources(Terra
        PRIVATE
            src/terra/window/glfw/window_glfw.cpp
    )

    if(UNIX)
        target_sources(Terra
            PRIVATE
                src/terra/window/glfw/window_glfw_linux.cpp
        )
    elseif(WIN32)
        target_sources(Terra
            PRIVATE
                src/terra/window/glfw/window_glfw_win32.cpp
        )
    endif()
endif()
