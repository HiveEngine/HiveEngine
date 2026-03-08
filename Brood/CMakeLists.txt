# ============================================================================
# Brood - Test Runner
# ============================================================================

# ============================================================================
# Test Discovery
# ============================================================================

set(LARVAE_TEST_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Hive/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Comb/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Wax/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Queen/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Nectar/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Waggle/tests
)

if(TARGET Terra)
    list(APPEND LARVAE_TEST_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../Terra/tests)
endif()

if(TARGET Swarm)
    list(APPEND LARVAE_TEST_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../Swarm/tests)
endif()

if(TARGET Antennae)
    list(APPEND LARVAE_TEST_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../Antennae/tests)
endif()

# Glob all test_*.cpp files from specified directories (recursive)
set(LARVAE_TEST_SOURCES src/larvae_runner_main.cpp)
foreach (TEST_DIR ${LARVAE_TEST_DIRS})
    file(GLOB_RECURSE DIR_TESTS CONFIGURE_DEPENDS ${TEST_DIR}/test_*.cpp)
    list(APPEND LARVAE_TEST_SOURCES ${DIR_TESTS})
endforeach ()

# ============================================================================
# Test Executable
# ============================================================================

add_executable(larvae_runner ${LARVAE_TEST_SOURCES})

hive_configure_target(larvae_runner)

target_link_libraries(larvae_runner
        PRIVATE
        Larvae
        Hive
        Comb
        Wax
        Queen
        Waggle
        Nectar
)

if(TARGET Terra)
    target_link_libraries(larvae_runner PRIVATE Terra)
endif()

if(TARGET Antennae)
    target_link_libraries(larvae_runner PRIVATE Antennae)
endif()

if(TARGET Swarm)
    target_link_libraries(larvae_runner PRIVATE Swarm)
endif()

target_include_directories(larvae_runner
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================================
# CTest Integration
# ============================================================================

enable_testing()
add_test(NAME AllTests COMMAND larvae_runner)

# ============================================================================
# Benchmark Discovery
# ============================================================================

# Glob all benchmark_*.cpp files from specified directories (recursive)
set(LARVAE_BENCHMARK_SOURCES src/larvae_benchmark_main.cpp)
foreach (TEST_DIR ${LARVAE_TEST_DIRS})
    file(GLOB_RECURSE DIR_BENCHMARKS CONFIGURE_DEPENDS ${TEST_DIR}/benchmark_*.cpp)
    list(APPEND LARVAE_BENCHMARK_SOURCES ${DIR_BENCHMARKS})
endforeach ()

# ============================================================================
# Benchmark Executable
# ============================================================================

add_executable(larvae_benchmark ${LARVAE_BENCHMARK_SOURCES})

hive_configure_target(larvae_benchmark)

target_link_libraries(larvae_benchmark
        PRIVATE
        Larvae
        Hive
        Comb
        Wax
        Queen
        Waggle
        Nectar
)

if(TARGET Terra)
    target_link_libraries(larvae_benchmark PRIVATE Terra)
endif()

if(TARGET Antennae)
    target_link_libraries(larvae_benchmark PRIVATE Antennae)
endif()

if(TARGET Swarm)
    target_link_libraries(larvae_benchmark PRIVATE Swarm)
endif()

target_include_directories(larvae_benchmark
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (TARGET Forge)
    target_link_libraries(hive_launcher PRIVATE Forge)
endif ()

add_dependencies(larvae_runner hive_launcher)

# ============================================================================
# GUI Test Runner (ImGui) - requires OpenGL, skip on headless builds
# ============================================================================

option(HIVE_BUILD_GUI_TESTS "Build the ImGui GUI test runner" ON)

if (HIVE_BUILD_GUI_TESTS AND HIVE_FEATURE_GLFW)

    find_package(OpenGL REQUIRED)

    include(FetchContent)

    # GLFW - only fetch if not already provided by Terra
    if (NOT TARGET glfw)
        FetchContent_Declare(
                glfw
                GIT_REPOSITORY https://github.com/glfw/glfw.git
                GIT_TAG 3.4
        )
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
    endif ()

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

    target_compile_options(imgui PRIVATE
            $<$<CXX_COMPILER_ID:Clang>:-w>
            $<$<CXX_COMPILER_ID:GNU>:-w>
            $<$<CXX_COMPILER_ID:MSVC>:/w>
    )

    # GUI Test sources (include all test files like console runner)
    set(LARVAE_GUI_SOURCES src/larvae_runner_gui_main.cpp)
    foreach (TEST_DIR ${LARVAE_TEST_DIRS})
        file(GLOB_RECURSE DIR_TESTS CONFIGURE_DEPENDS ${TEST_DIR}/test_*.cpp)
        list(APPEND LARVAE_GUI_SOURCES ${DIR_TESTS})
    endforeach ()

    # GUI Executable
    add_executable(larvae_runner_gui ${LARVAE_GUI_SOURCES})

    hive_configure_target(larvae_runner_gui)

    target_link_libraries(larvae_runner_gui
            PRIVATE
            Larvae
            Hive
            Comb
            Wax
            Queen
            Waggle
            Nectar
            imgui
    )

    if(TARGET Terra)
        target_link_libraries(larvae_runner_gui PRIVATE Terra)
    endif()

    if(TARGET Antennae)
        target_link_libraries(larvae_runner_gui PRIVATE Antennae)
    endif()

    if(TARGET Swarm)
        target_link_libraries(larvae_runner_gui PRIVATE Swarm)
    endif()

    target_include_directories(larvae_runner_gui
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # On Windows, set WIN32 subsystem to avoid console window
    if (WIN32)
        set_target_properties(larvae_runner_gui
                PROPERTIES
                WIN32_EXECUTABLE TRUE
        )
    endif ()

endif () # HIVE_BUILD_GUI_TESTS
