cmake_minimum_required(VERSION 3.28)

# Match the main project setup for Clang/MSVC on Windows.
if(WIN32)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
endif()

if(WIN32 AND NOT CMAKE_RC_COMPILER)
    file(GLOB _hive_diligent_rc_paths
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/rc.exe"
        "C:/Program Files (x86)/Windows Kits/*/bin/x64/rc.exe"
    )
    if(_hive_diligent_rc_paths)
        list(GET _hive_diligent_rc_paths 0 _hive_diligent_found_rc)
        set(CMAKE_RC_COMPILER "${_hive_diligent_found_rc}" CACHE FILEPATH "RC Compiler")
    else()
        set(CMAKE_RC_COMPILER "rc" CACHE FILEPATH "RC Compiler")
    endif()
endif()

project(HiveDiligentBootstrap LANGUAGES C CXX)

set(HIVE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "HiveEngine source root")
get_filename_component(HIVE_ROOT_DIR "${HIVE_ROOT_DIR}" ABSOLUTE)

set(HIVE_DILIGENT_SOURCE_DIR "${HIVE_ROOT_DIR}/Swarm/extern/diligentcore")
set(HIVE_DILIGENT_EXPORT_DIR "${CMAKE_BINARY_DIR}/package" CACHE PATH "Directory where HiveDiligentConfig.cmake is generated")

if(NOT EXISTS "${HIVE_DILIGENT_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Diligent source directory not found at ${HIVE_DILIGENT_SOURCE_DIR}")
endif()

if(WIN32 AND NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    file(GLOB _hive_diligent_sdk_versions "C:/Program Files (x86)/Windows Kits/10/bin/10.*")
    if(_hive_diligent_sdk_versions)
        list(SORT _hive_diligent_sdk_versions COMPARE NATURAL ORDER DESCENDING)
        list(GET _hive_diligent_sdk_versions 0 _hive_diligent_sdk_latest)
        cmake_path(GET _hive_diligent_sdk_latest FILENAME _hive_diligent_sdk_ver)
        set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "${_hive_diligent_sdk_ver}" CACHE STRING "" FORCE)
    endif()
endif()

set(DILIGENT_NO_FORMAT_VALIDATION ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_DIRECT3D11 ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_DIRECT3D12 ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_METAL ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_WEBGPU ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_ARCHIVER ON CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)

add_subdirectory("${HIVE_DILIGENT_SOURCE_DIR}" diligentcore)

if(NOT TARGET Diligent-GraphicsEngineVk-static)
    message(FATAL_ERROR "Expected target Diligent-GraphicsEngineVk-static was not generated")
endif()

add_custom_target(HiveDiligentPackage
    DEPENDS
        Diligent-GraphicsEngineVk-static
)

function(hive_diligent_collect_targets dir out_var)
    set(_targets "")

    get_property(_dir_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND _targets ${_dir_targets})

    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_subdir IN LISTS _subdirs)
        hive_diligent_collect_targets("${_subdir}" _subdir_targets)
        list(APPEND _targets ${_subdir_targets})
    endforeach()

    list(REMOVE_DUPLICATES _targets)
    set(${out_var} ${_targets} PARENT_SCOPE)
endfunction()

hive_diligent_collect_targets("${CMAKE_BINARY_DIR}/diligentcore" HIVE_DILIGENT_ALL_TARGETS)

set(HIVE_DILIGENT_EXPORT_TARGETS "")
foreach(_target IN LISTS HIVE_DILIGENT_ALL_TARGETS)
    get_target_property(_aliased_target "${_target}" ALIASED_TARGET)
    if(_aliased_target)
        continue()
    endif()

    get_target_property(_imported_target "${_target}" IMPORTED)
    if(_imported_target)
        continue()
    endif()

    get_target_property(_target_type "${_target}" TYPE)
    if(_target_type STREQUAL "STATIC_LIBRARY" OR
       _target_type STREQUAL "INTERFACE_LIBRARY")
        list(APPEND HIVE_DILIGENT_EXPORT_TARGETS "${_target}")
    endif()
endforeach()

list(REMOVE_DUPLICATES HIVE_DILIGENT_EXPORT_TARGETS)
list(SORT HIVE_DILIGENT_EXPORT_TARGETS)

file(MAKE_DIRECTORY "${HIVE_DILIGENT_EXPORT_DIR}")

export(TARGETS ${HIVE_DILIGENT_EXPORT_TARGETS}
    FILE "${HIVE_DILIGENT_EXPORT_DIR}/HiveDiligentTargets.cmake"
)

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/HiveDiligentConfig.cmake.in"
    "${HIVE_DILIGENT_EXPORT_DIR}/HiveDiligentConfig.cmake"
    @ONLY
)

file(WRITE "${HIVE_DILIGENT_EXPORT_DIR}/HiveDiligentConfigVersion.cmake"
    "set(PACKAGE_VERSION \"1.0.0\")\n"
    "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
    "set(PACKAGE_VERSION_EXACT TRUE)\n"
)

message(STATUS "HiveDiligent package generated at ${HIVE_DILIGENT_EXPORT_DIR}")
