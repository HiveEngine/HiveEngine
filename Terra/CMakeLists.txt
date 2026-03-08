# ============================================================================
# Terra - Windowing Module
# ============================================================================

add_library(Terra STATIC)

hive_configure_target(Terra)

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

add_subdirectory(extern/glfw SYSTEM)

if(MSVC)
    target_compile_options(glfw PRIVATE /w)
else()
    target_compile_options(glfw PRIVATE -w)
endif()

target_link_libraries(Terra
    PUBLIC
        glfw
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

target_include_directories(Terra
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/glfw/include
)
