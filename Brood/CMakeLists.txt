# ============================================================================
# Brood - Test Runner
# ============================================================================

set(BROOD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(BROOD_TEST_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Hive/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Comb/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Wax/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Queen/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Nectar/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Waggle/tests
)

foreach(_module IN ITEMS Terra Swarm Antennae)
    if(TARGET ${_module})
        list(APPEND BROOD_TEST_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../${_module}/tests)
    endif()
endforeach()

function(brood_collect_sources out_var entrypoint pattern)
    set(_sources ${entrypoint})
    foreach(_test_dir IN LISTS BROOD_TEST_DIRS)
        file(GLOB_RECURSE _dir_sources CONFIGURE_DEPENDS "${_test_dir}/${pattern}")
        list(APPEND _sources ${_dir_sources})
    endforeach()
    set(${out_var} ${_sources} PARENT_SCOPE)
endfunction()

function(brood_link_test_executable target)
    target_link_libraries(${target}
        PRIVATE
            Larvae
            Hive
            Comb
            Wax
            Queen
            Waggle
            Nectar
    )

    foreach(_module IN ITEMS Terra Antennae Swarm)
        if(TARGET ${_module})
            target_link_libraries(${target} PRIVATE ${_module})
        endif()
    endforeach()

    target_include_directories(${target}
        PRIVATE
            ${BROOD_INCLUDE_DIR}
    )
endfunction()

# ============================================================================
# Test Executable
# ============================================================================

brood_collect_sources(BROOD_RUNNER_SOURCES src/larvae_runner_main.cpp test_*.cpp)

add_executable(larvae_runner ${BROOD_RUNNER_SOURCES})

hive_configure_target(larvae_runner)

brood_link_test_executable(larvae_runner)

# ============================================================================
# CTest Integration
# ============================================================================

enable_testing()
add_test(NAME AllTests COMMAND larvae_runner)

# ============================================================================
# Benchmark Discovery
# ============================================================================

brood_collect_sources(BROOD_BENCHMARK_SOURCES src/larvae_benchmark_main.cpp benchmark_*.cpp)

# ============================================================================
# Benchmark Executable
# ============================================================================

add_executable(larvae_benchmark ${BROOD_BENCHMARK_SOURCES})

hive_configure_target(larvae_benchmark)

brood_link_test_executable(larvae_benchmark)

# ============================================================================
# Engine Launcher
# ============================================================================

add_executable(hive_launcher src/hive_launcher.cpp)

hive_configure_target(hive_launcher)

target_link_libraries(hive_launcher
    PRIVATE
        Waggle
        Hive
        Comb
        Nectar
)

target_include_directories(hive_launcher
    PRIVATE
        ${BROOD_INCLUDE_DIR}
)

if(TARGET Forge)
    target_link_libraries(hive_launcher PRIVATE Forge)
endif()

add_dependencies(larvae_runner hive_launcher)

# ============================================================================
# GUI Test Runner (ImGui) - requires OpenGL, skip on headless builds
# ============================================================================

option(HIVE_BUILD_GUI_TESTS "Build the ImGui GUI test runner" ON)

if(HIVE_BUILD_GUI_TESTS AND HIVE_FEATURE_GLFW)

    find_package(OpenGL REQUIRED)

    include(FetchContent)

    # GLFW - only fetch if not already provided by Terra
    if(NOT TARGET glfw)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
        )
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
    endif()

    # ImGui
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.6
    )
    FetchContent_MakeAvailable(imgui)

    # ImGui library
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui PUBLIC
        glfw
        OpenGL::GL
    )

    hive_silence_target_warnings(imgui)

    brood_collect_sources(BROOD_GUI_SOURCES src/larvae_runner_gui_main.cpp test_*.cpp)

    add_executable(larvae_runner_gui ${BROOD_GUI_SOURCES})

    hive_configure_target(larvae_runner_gui)

    brood_link_test_executable(larvae_runner_gui)

    target_link_libraries(larvae_runner_gui
        PRIVATE
            imgui
    )

    # On Windows, set WIN32 subsystem to avoid console window
    if(WIN32)
        set_target_properties(larvae_runner_gui
            PROPERTIES
                WIN32_EXECUTABLE TRUE
        )
    endif()

endif()
