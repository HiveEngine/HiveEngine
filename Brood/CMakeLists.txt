# ============================================================================
# Brood - Tools and Test Runner
# ============================================================================

set(BROOD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(BROOD_CMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

function(brood_link_test_target target)
    if(HIVE_BUILD_SHARED)
        target_link_libraries(${target} PRIVATE Larvae hive_engine)
    else()
        target_link_libraries(${target}
            PRIVATE
                Larvae
                Hive
                Comb
                Wax
                Queen
                Waggle
                Nectar
                Drone
        )

        foreach(_module IN ITEMS Terra Antennae Swarm Forge)
            if(TARGET ${_module})
                target_link_libraries(${target} PRIVATE ${_module})
            endif()
        endforeach()
    endif()

    target_include_directories(${target}
        PRIVATE
            ${BROOD_INCLUDE_DIR}
    )
endfunction()

# ============================================================================
# Engine Launcher
# ============================================================================

add_executable(hive_launcher
    src/hive_launcher.cpp
    src/launcher/gltf_import_job.cpp
    src/launcher/launcher_platform.cpp
    src/launcher/launcher_scene.cpp
    src/launcher/launcher_project.cpp
)

hive_configure_target(hive_launcher)

if(HIVE_BUILD_SHARED)
    target_link_libraries(hive_launcher PRIVATE hive_engine)
else()
    target_link_libraries(hive_launcher PRIVATE Waggle Hive Comb Nectar)
endif()

target_include_directories(hive_launcher
    PRIVATE
        ${BROOD_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(TARGET Forge)
    target_link_libraries(hive_launcher PRIVATE Forge)
endif()

if(WIN32)
    target_link_libraries(hive_launcher PRIVATE Ole32 Shell32)
endif()

if(HIVE_BUILD_TESTING)

    set(BROOD_TEST_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Larvae/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Hive/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Comb/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Wax/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Drone/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Queen/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Nectar/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Waggle/tests
    )

    set(BROOD_BENCHMARK_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Larvae/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Hive/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Comb/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Wax/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Queen/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Nectar/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Waggle/tests
    )

    if(TARGET Swarm)
        list(APPEND BROOD_TEST_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../Swarm/tests)
        list(APPEND BROOD_BENCHMARK_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../Swarm/tests)
    endif()

    # Some module tests include GLFW-specific headers directly.
    # Keep the headless runner buildable by collecting them only when windowing is enabled.
    if(HIVE_FEATURE_GLFW)
        foreach(_module IN ITEMS Terra Antennae)
            if(TARGET ${_module})
                list(APPEND BROOD_TEST_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../${_module}/tests)
                list(APPEND BROOD_BENCHMARK_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../${_module}/tests)
            endif()
        endforeach()
    endif()

    if(TARGET Forge)
        list(APPEND BROOD_TEST_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../Forge/tests)
    endif()

    function(brood_collect_sources out_var entrypoint pattern)
        set(options)
        set(one_value_args DIR_LIST)
        cmake_parse_arguments(BROOD_COLLECT "${options}" "${one_value_args}" "" ${ARGN})

        set(_sources ${entrypoint})
        foreach(_test_dir IN LISTS ${BROOD_COLLECT_DIR_LIST})
            file(GLOB_RECURSE _dir_sources CONFIGURE_DEPENDS "${_test_dir}/${pattern}")
            list(APPEND _sources ${_dir_sources})
        endforeach()
        set(${out_var} ${_sources} PARENT_SCOPE)
    endfunction()

    # ============================================================================
    # Test Executable
    # ============================================================================

    brood_collect_sources(BROOD_TEST_SOURCES "" test_*.cpp DIR_LIST BROOD_TEST_DIRS)

    add_library(brood_tests OBJECT ${BROOD_TEST_SOURCES})

    hive_configure_target(brood_tests)

    brood_link_test_target(brood_tests)

    add_executable(larvae_runner
        src/larvae_runner_main.cpp
        $<TARGET_OBJECTS:brood_tests>
    )

    hive_configure_target(larvae_runner)

    brood_link_test_target(larvae_runner)

    add_dependencies(larvae_runner hive_launcher)

    # ============================================================================
    # CTest Integration
    # ============================================================================

    enable_testing()
    add_test(NAME AllTests COMMAND larvae_runner --fail-on-skip --exclude-suite=LarvaeCapabilitiesSkip)
    set_tests_properties(AllTests PROPERTIES LABELS "runner;ci")

    # ============================================================================
    # Benchmark Discovery
    # ============================================================================

    brood_collect_sources(BROOD_BENCHMARK_SOURCES src/larvae_benchmark_main.cpp benchmark_*.cpp DIR_LIST
                          BROOD_BENCHMARK_DIRS)

    # ============================================================================
    # Benchmark Executable
    # ============================================================================

    add_executable(larvae_benchmark ${BROOD_BENCHMARK_SOURCES})

    hive_configure_target(larvae_benchmark)

    brood_link_test_target(larvae_benchmark)

    add_test(NAME LarvaeRunnerCapabilityList COMMAND larvae_runner --suite=LarvaeCapabilitiesSkip --list)
    set_tests_properties(LarvaeRunnerCapabilityList PROPERTIES LABELS "runner;capabilities")

    add_test(NAME LarvaeRunnerStrictSkipRegression
        COMMAND ${CMAKE_COMMAND}
            -DTEST_EXECUTABLE=$<TARGET_FILE:larvae_runner>
            -DTEST_ARGS=--suite=LarvaeCapabilitiesSkip|--capabilities=headless|--fail-on-skip
            -DEXPECT_OUTPUT_REGEX=missing capabilities: window
            -P ${BROOD_CMAKE_DIR}/run_expected_failure.cmake
    )
    set_tests_properties(LarvaeRunnerStrictSkipRegression
        PROPERTIES
            LABELS "runner;capabilities;regression"
    )

    add_test(NAME LarvaeBenchmarkCapabilityList COMMAND larvae_benchmark --benchmark-filter=LarvaeCapabilities* --list
                                                              --capabilities=headless)
    set_tests_properties(LarvaeBenchmarkCapabilityList
        PROPERTIES
            LABELS "benchmark;capabilities"
            PASS_REGULAR_EXPRESSION "LarvaeCapabilities.WindowOnly"
    )

    add_test(NAME LarvaeBenchmarkStrictSkipRegression
        COMMAND ${CMAKE_COMMAND}
            -DTEST_EXECUTABLE=$<TARGET_FILE:larvae_benchmark>
            -DTEST_ARGS=--benchmark-filter=LarvaeCapabilities*|--list|--capabilities=headless|--fail-on-skip
            -DEXPECT_OUTPUT_REGEX=LarvaeCapabilities.WindowOnly
            -P ${BROOD_CMAKE_DIR}/run_expected_failure.cmake
    )
    set_tests_properties(LarvaeBenchmarkStrictSkipRegression
        PROPERTIES
            LABELS "benchmark;capabilities;regression"
    )

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

        add_executable(larvae_runner_gui
            src/larvae_runner_gui_main.cpp
            $<TARGET_OBJECTS:brood_tests>
        )

        hive_configure_target(larvae_runner_gui)

        brood_link_test_target(larvae_runner_gui)

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
endif()
