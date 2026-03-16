include_guard(GLOBAL)

include(CMakeParseArguments)

function(hive_add_game_project)
    set(options LINK_SWARM LINK_TERRA LINK_ANTENNAE)
    set(oneValueArgs TARGET PROJECT_ROOT PROJECT_FILE SOURCE_INCLUDE_DIR)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(HIVE_GAME_PROJECT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT HIVE_GAME_PROJECT_TARGET)
        set(HIVE_GAME_PROJECT_TARGET gameplay)
    endif()

    if(NOT HIVE_GAME_PROJECT_PROJECT_ROOT)
        set(HIVE_GAME_PROJECT_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    if(NOT HIVE_GAME_PROJECT_PROJECT_FILE)
        set(HIVE_GAME_PROJECT_PROJECT_FILE "${HIVE_GAME_PROJECT_PROJECT_ROOT}/project.hive")
    endif()

    if(HIVE_GAME_PROJECT_SOURCES)
        add_library(${HIVE_GAME_PROJECT_TARGET} MODULE ${HIVE_GAME_PROJECT_SOURCES})

        set(_gameplay_args
            PROJECT_ROOT "${HIVE_GAME_PROJECT_PROJECT_ROOT}"
        )

        if(HIVE_GAME_PROJECT_SOURCE_INCLUDE_DIR)
            list(APPEND _gameplay_args SOURCE_INCLUDE_DIR "${HIVE_GAME_PROJECT_SOURCE_INCLUDE_DIR}")
        endif()

        if(HIVE_GAME_PROJECT_LINK_SWARM)
            list(APPEND _gameplay_args LINK_SWARM)
        endif()

        if(HIVE_GAME_PROJECT_LINK_TERRA)
            list(APPEND _gameplay_args LINK_TERRA)
        endif()

        if(HIVE_GAME_PROJECT_LINK_ANTENNAE)
            list(APPEND _gameplay_args LINK_ANTENNAE)
        endif()

        hive_configure_gameplay_module(${HIVE_GAME_PROJECT_TARGET}
            ${_gameplay_args}
        )

        hive_add_project_launcher_targets(
            PROJECT_ROOT "${HIVE_GAME_PROJECT_PROJECT_ROOT}"
            PROJECT_FILE "${HIVE_GAME_PROJECT_PROJECT_FILE}"
            GAMEPLAY_TARGET ${HIVE_GAME_PROJECT_TARGET}
        )
        return()
    endif()

    hive_add_project_launcher_targets(
        PROJECT_ROOT "${HIVE_GAME_PROJECT_PROJECT_ROOT}"
        PROJECT_FILE "${HIVE_GAME_PROJECT_PROJECT_FILE}"
    )
endfunction()

function(hive_configure_gameplay_module target)
    set(options LINK_SWARM LINK_TERRA LINK_ANTENNAE)
    set(oneValueArgs PROJECT_ROOT SOURCE_INCLUDE_DIR)
    cmake_parse_arguments(HIVE_GAMEPLAY "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT TARGET ${target})
        message(FATAL_ERROR "Unknown gameplay target '${target}'")
    endif()

    if(NOT HIVE_GAMEPLAY_PROJECT_ROOT)
        set(HIVE_GAMEPLAY_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    if(NOT HIVE_GAMEPLAY_SOURCE_INCLUDE_DIR)
        set(HIVE_GAMEPLAY_SOURCE_INCLUDE_DIR "${HIVE_GAMEPLAY_PROJECT_ROOT}/src")
    endif()

    hive_configure_target(${target})

    target_link_libraries(${target}
        PRIVATE
            Queen
            Waggle
            Hive
            Comb
            Wax
            Nectar
    )

    if(HIVE_GAMEPLAY_LINK_SWARM AND TARGET Swarm)
        target_link_libraries(${target} PRIVATE Swarm)
    endif()

    if(HIVE_GAMEPLAY_LINK_TERRA AND TARGET Terra)
        target_link_libraries(${target} PRIVATE Terra)
    endif()

    if(HIVE_GAMEPLAY_LINK_ANTENNAE AND TARGET Antennae)
        target_link_libraries(${target} PRIVATE Antennae)
    endif()

    target_include_directories(${target} PRIVATE "${HIVE_GAMEPLAY_SOURCE_INCLUDE_DIR}")

    set_target_properties(${target}
        PROPERTIES
            PREFIX ""
            OUTPUT_NAME "gameplay"
            LIBRARY_OUTPUT_DIRECTORY "${HIVE_GAMEPLAY_PROJECT_ROOT}"
            RUNTIME_OUTPUT_DIRECTORY "${HIVE_GAMEPLAY_PROJECT_ROOT}"
    )

    foreach(_config IN ITEMS Debug Profile Retail Release RelWithDebInfo)
        string(TOUPPER "${_config}" _config_upper)
        set_target_properties(${target}
            PROPERTIES
                "LIBRARY_OUTPUT_DIRECTORY_${_config_upper}" "${HIVE_GAMEPLAY_PROJECT_ROOT}"
                "RUNTIME_OUTPUT_DIRECTORY_${_config_upper}" "${HIVE_GAMEPLAY_PROJECT_ROOT}"
        )
    endforeach()
endfunction()

function(hive_add_project_launcher_targets)
    set(oneValueArgs PROJECT_ROOT PROJECT_FILE GAMEPLAY_TARGET)
    cmake_parse_arguments(HIVE_PROJECT "" "${oneValueArgs}" "" ${ARGN})

    if(NOT TARGET hive_launcher)
        message(FATAL_ERROR "hive_add_project_launcher_targets() requires target 'hive_launcher'")
    endif()

    if(NOT HIVE_PROJECT_PROJECT_ROOT)
        set(HIVE_PROJECT_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    if(NOT HIVE_PROJECT_PROJECT_FILE)
        set(HIVE_PROJECT_PROJECT_FILE "${HIVE_PROJECT_PROJECT_ROOT}/project.hive")
    endif()

    if(HIVE_PROJECT_GAMEPLAY_TARGET)
        if(NOT TARGET ${HIVE_PROJECT_GAMEPLAY_TARGET})
            message(FATAL_ERROR "Unknown gameplay target '${HIVE_PROJECT_GAMEPLAY_TARGET}'")
        endif()

        add_dependencies(hive_launcher ${HIVE_PROJECT_GAMEPLAY_TARGET})

        if(TARGET larvae_runner)
            add_dependencies(larvae_runner ${HIVE_PROJECT_GAMEPLAY_TARGET})
        endif()
    endif()

    set(_mode_target "hive_run_game")
    set(_mode_args --game --project "${HIVE_PROJECT_PROJECT_FILE}")

    if(HIVE_ENGINE_MODE STREQUAL "Editor")
        set(_mode_target "hive_run_editor")
        set(_mode_args --editor --project "${HIVE_PROJECT_PROJECT_FILE}")
    elseif(HIVE_ENGINE_MODE STREQUAL "Headless")
        set(_mode_target "hive_run_headless")
        set(_mode_args --headless --project "${HIVE_PROJECT_PROJECT_FILE}")
    endif()

    foreach(_target_name IN ITEMS hive_run_project ${_mode_target})
        if(TARGET ${_target_name})
            continue()
        endif()

        add_custom_target(${_target_name}
            COMMAND ${CMAKE_COMMAND} -E chdir "${HIVE_PROJECT_PROJECT_ROOT}" $<TARGET_FILE:hive_launcher> ${_mode_args}
            DEPENDS hive_launcher
            USES_TERMINAL
            VERBATIM
        )
    endforeach()
endfunction()
