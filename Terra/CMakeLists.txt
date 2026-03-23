# ============================================================================
# Terra - Windowing Module
# ============================================================================

add_library(Terra ${HIVE_LIB_TYPE})

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


target_sources(Terra
        PRIVATE
        src/terra/terralog.cpp
        src/terra/terramodule.cpp
)

target_link_libraries(Terra
        PUBLIC
        Hive
        Comb
)


if(HIVE_FEATURE_GLFW)
    add_subdirectory(extern/glfw SYSTEM)

    hive_silence_target_warnings(glfw)

    target_link_libraries(Terra
        PUBLIC
            glfw
    )

    target_sources(Terra
        PRIVATE
            src/terra/platform/glfw/glfw_terra.cpp
    )

    if(UNIX)
        target_sources(Terra PRIVATE src/terra/platform/glfw/linux_glfw_terra.cpp)
    endif()

    if(WIN32)
        target_sources(Terra PRIVATE src/terra/platform/glfw/win32_glfw_terra.cpp)
    endif()

    target_include_directories(Terra
        SYSTEM PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/extern/glfw/include
    )
else()
    target_sources(Terra
        PRIVATE
            src/terra/platform/nullop/null_terra.cpp
    )
endif()
