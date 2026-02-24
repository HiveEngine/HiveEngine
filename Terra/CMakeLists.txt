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
            src/terra/glfw_terra.cpp
    )

    if(UNIX)
        target_sources(Terra PRIVATE src/terra/linux_glfw_terra.cpp)
    endif ()

    if(WIN32)
        target_sources(Terra PRIVATE src/terra/win32_glfw_terra.cpp)
    endif ()
endif()
