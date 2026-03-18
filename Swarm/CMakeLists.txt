# ============================================================================
# Swarm - RHI for HiveEngine
# ============================================================================

add_library(Swarm STATIC)

target_include_directories(Swarm
    PUBLIC
        include
)

if(HIVE_HAS_RENDER_BACKEND)

hive_configure_target(Swarm RTTI ON EXCEPTIONS ON)

get_filename_component(_swarm_engine_root_dir "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# Re-enable exceptions and suppress warnings for all Diligent targets built from source
function(swarm_fix_diligent_targets DIR)
    get_property(TARGETS DIRECTORY "${DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(TGT ${TARGETS})
        get_target_property(TGT_TYPE ${TGT} TYPE)
        get_target_property(TGT_IMPORTED ${TGT} IMPORTED)
        if(TGT_TYPE STREQUAL "INTERFACE_LIBRARY" OR TGT_TYPE STREQUAL "UTILITY" OR TGT_IMPORTED)
            continue()
        endif()
        if(MSVC)
            target_compile_options(${TGT} PRIVATE /EHsc /GR)
        else()
            target_compile_options(${TGT} PRIVATE -fexceptions -frtti)
        endif()
        hive_silence_target_warnings(${TGT})
    endforeach()
    get_property(SUBDIRS DIRECTORY "${DIR}" PROPERTY SUBDIRECTORIES)
    foreach(SUBDIR ${SUBDIRS})
        swarm_fix_diligent_targets("${SUBDIR}")
    endforeach()
endfunction()

function(swarm_find_prebuilt_diligent PACKAGE_DIR OUT_FOUND OUT_SOURCE_DIR)
    set(_swarm_had_find_targets_global FALSE)
    if(DEFINED CMAKE_FIND_PACKAGE_TARGETS_GLOBAL)
        set(_swarm_had_find_targets_global TRUE)
        set(_swarm_prev_find_targets_global "${CMAKE_FIND_PACKAGE_TARGETS_GLOBAL}")
    endif()

    set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)
    find_package(HiveDiligent CONFIG QUIET
        PATHS "${PACKAGE_DIR}"
        NO_DEFAULT_PATH
    )

    if(_swarm_had_find_targets_global)
        set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL "${_swarm_prev_find_targets_global}")
    else()
        unset(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL)
    endif()

    unset(_swarm_had_find_targets_global)
    unset(_swarm_prev_find_targets_global)

    if(HiveDiligent_FOUND)
        set(${OUT_FOUND} TRUE PARENT_SCOPE)
        if(HIVE_DILIGENT_SOURCE_DIR)
            set(${OUT_SOURCE_DIR} "${HIVE_DILIGENT_SOURCE_DIR}" PARENT_SCOPE)
        else()
            set(${OUT_SOURCE_DIR} "" PARENT_SCOPE)
        endif()
    else()
        set(${OUT_FOUND} FALSE PARENT_SCOPE)
        set(${OUT_SOURCE_DIR} "" PARENT_SCOPE)
    endif()
endfunction()

function(swarm_bootstrap_prebuilt_diligent BUILD_DIR PACKAGE_DIR BUILD_CONFIG)
    set(_swarm_bootstrap_source_dir "${_swarm_engine_root_dir}/tools/diligent")
    if(NOT EXISTS "${_swarm_bootstrap_source_dir}/CMakeLists.txt")
        message(FATAL_ERROR "Swarm: Diligent bootstrap project not found at ${_swarm_bootstrap_source_dir}")
    endif()

    file(MAKE_DIRECTORY "${BUILD_DIR}")
    file(MAKE_DIRECTORY "${PACKAGE_DIR}")

    set(_swarm_bootstrap_configure_stdout_log "${BUILD_DIR}/hive_diligent_configure.stdout.log")
    set(_swarm_bootstrap_configure_stderr_log "${BUILD_DIR}/hive_diligent_configure.stderr.log")
    set(_swarm_bootstrap_build_stdout_log "${BUILD_DIR}/hive_diligent_build.stdout.log")
    set(_swarm_bootstrap_build_stderr_log "${BUILD_DIR}/hive_diligent_build.stderr.log")

    set(_swarm_bootstrap_configure_cmd
        "${CMAKE_COMMAND}"
        -S "${_swarm_bootstrap_source_dir}"
        -B "${BUILD_DIR}"
        -G "${CMAKE_GENERATOR}"
        "-DHIVE_ROOT_DIR=${_swarm_engine_root_dir}"
        "-DHIVE_DILIGENT_EXPORT_DIR=${PACKAGE_DIR}"
    )

    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND _swarm_bootstrap_configure_cmd -A "${CMAKE_GENERATOR_PLATFORM}")
    endif()

    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND _swarm_bootstrap_configure_cmd -T "${CMAKE_GENERATOR_TOOLSET}")
    endif()

    if(CMAKE_MAKE_PROGRAM)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
    endif()

    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif()

    if(CMAKE_BUILD_TYPE)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    endif()

    if(CMAKE_C_COMPILER)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    endif()

    if(CMAKE_CXX_COMPILER)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
    endif()

    if(CMAKE_C_COMPILER_LAUNCHER)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}")
    endif()

    if(CMAKE_CXX_COMPILER_LAUNCHER)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}")
    endif()

    if(CMAKE_RC_COMPILER)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_RC_COMPILER=${CMAKE_RC_COMPILER}")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    if(CMAKE_MSVC_RUNTIME_LIBRARY)
        list(APPEND _swarm_bootstrap_configure_cmd "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()

    message(STATUS "Swarm: bootstrapping prebuilt Diligent in ${BUILD_DIR}")

    execute_process(
        COMMAND ${_swarm_bootstrap_configure_cmd}
        RESULT_VARIABLE _swarm_bootstrap_configure_result
        OUTPUT_FILE "${_swarm_bootstrap_configure_stdout_log}"
        ERROR_FILE "${_swarm_bootstrap_configure_stderr_log}"
    )

    if(NOT _swarm_bootstrap_configure_result EQUAL 0)
        message(FATAL_ERROR
            "Swarm: failed to configure the Diligent bootstrap project. "
            "See ${_swarm_bootstrap_configure_stdout_log} and ${_swarm_bootstrap_configure_stderr_log}"
        )
    endif()

    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            --build "${BUILD_DIR}"
            --target HiveDiligentPackage
            --config "${BUILD_CONFIG}"
        RESULT_VARIABLE _swarm_bootstrap_build_result
        OUTPUT_FILE "${_swarm_bootstrap_build_stdout_log}"
        ERROR_FILE "${_swarm_bootstrap_build_stderr_log}"
    )

    if(NOT _swarm_bootstrap_build_result EQUAL 0)
        message(FATAL_ERROR
            "Swarm: failed to build the prebuilt Diligent package. "
            "See ${_swarm_bootstrap_build_stdout_log} and ${_swarm_bootstrap_build_stderr_log}"
        )
    endif()
endfunction()

set(_swarm_diligent_provider "${HIVE_DILIGENT_PROVIDER}")
if(NOT _swarm_diligent_provider)
    set(_swarm_diligent_provider "AUTO")
endif()
string(TOUPPER "${_swarm_diligent_provider}" _swarm_diligent_provider)

set(_swarm_diligent_build_config "${CMAKE_BUILD_TYPE}")
if(NOT _swarm_diligent_build_config)
    set(_swarm_diligent_build_config "Debug")
endif()

set(_swarm_diligent_package_dir "${HIVE_DILIGENT_PACKAGE_DIR}")
if(NOT _swarm_diligent_package_dir)
    string(TOLOWER "${_swarm_diligent_build_config}" _swarm_diligent_config_suffix)
    if(NOT _swarm_diligent_config_suffix)
        set(_swarm_diligent_config_suffix "debug")
    endif()
    set(_swarm_diligent_package_dir
        "${_swarm_engine_root_dir}/out/build/diligent-${_swarm_diligent_config_suffix}/package"
    )
endif()

set(_swarm_diligent_bootstrap_build_dir "${HIVE_DILIGENT_BOOTSTRAP_BUILD_DIR}")
if(NOT _swarm_diligent_bootstrap_build_dir)
    string(TOLOWER "${_swarm_diligent_build_config}" _swarm_diligent_bootstrap_suffix)
    set(_swarm_diligent_bootstrap_build_dir
        "${_swarm_engine_root_dir}/out/build/diligent-${_swarm_diligent_bootstrap_suffix}"
    )
endif()

set(_swarm_diligent_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/extern/diligentcore")
set(_swarm_using_prebuilt_diligent OFF)

if(_swarm_diligent_provider STREQUAL "AUTO" OR _swarm_diligent_provider STREQUAL "PREBUILT")
    swarm_find_prebuilt_diligent(
        "${_swarm_diligent_package_dir}"
        _swarm_using_prebuilt_diligent
        _swarm_prebuilt_diligent_source_dir
    )

    if(NOT _swarm_using_prebuilt_diligent AND HIVE_BOOTSTRAP_DILIGENT)
        swarm_bootstrap_prebuilt_diligent(
            "${_swarm_diligent_bootstrap_build_dir}"
            "${_swarm_diligent_package_dir}"
            "${_swarm_diligent_build_config}"
        )
        swarm_find_prebuilt_diligent(
            "${_swarm_diligent_package_dir}"
            _swarm_using_prebuilt_diligent
            _swarm_prebuilt_diligent_source_dir
        )
    endif()

    if(_swarm_using_prebuilt_diligent)
        if(_swarm_prebuilt_diligent_source_dir)
            set(_swarm_diligent_source_dir "${_swarm_prebuilt_diligent_source_dir}")
        endif()
        message(STATUS "Swarm: using prebuilt Diligent from ${_swarm_diligent_package_dir}")
    elseif(_swarm_diligent_provider STREQUAL "PREBUILT" OR HIVE_BOOTSTRAP_DILIGENT)
        message(FATAL_ERROR
            "Swarm: prebuilt Diligent is unavailable in ${_swarm_diligent_package_dir}. "
            "Automatic bootstrap was requested, but no HiveDiligent package was produced."
        )
    endif()
endif()

if(NOT _swarm_using_prebuilt_diligent)
    # DiligentCore needs exceptions enabled and relaxed warnings.
    # Only build the backends we actually use.
    # Auto-detect Windows SDK version for DiligentCore (works for any machine).
    if(WIN32 AND NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        file(GLOB _SDK_VERSIONS "C:/Program Files (x86)/Windows Kits/10/bin/10.*")
        if(_SDK_VERSIONS)
            list(SORT _SDK_VERSIONS COMPARE NATURAL ORDER DESCENDING)
            list(GET _SDK_VERSIONS 0 _SDK_LATEST)
            cmake_path(GET _SDK_LATEST FILENAME _SDK_VER)
            set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "${_SDK_VER}" CACHE STRING "" FORCE)
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
    add_subdirectory(extern/diligentcore SYSTEM)
    swarm_fix_diligent_targets("${CMAKE_CURRENT_SOURCE_DIR}/extern/diligentcore")
    message(STATUS "Swarm: using source Diligent fallback from ${_swarm_diligent_source_dir}")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND TARGET Diligent-BuildSettings)
    # Diligent exports its own warning/optimization policy through an interface
    # target. Keep its compile definitions/includes, but let the engine own the
    # warning profile of first-party TUs.
    set_target_properties(Diligent-BuildSettings PROPERTIES INTERFACE_COMPILE_OPTIONS "")
endif()

target_sources(Swarm
    PRIVATE
        src/swarm.cpp
        src/swarmmodule.cpp
        src/platform/diligent/diligent_swarm.cpp
        src/platform/diligent/diligent_swarm_vulkan.cpp
)

if(UNIX)
    target_sources(Swarm PRIVATE src/platform/diligent/diligent_swarm_linux_vulkan.cpp)
endif()

if(WIN32)
    target_sources(Swarm PRIVATE src/platform/diligent/diligent_swarm_win32_vulkan.cpp)
endif()

target_include_directories(Swarm
    SYSTEM PUBLIC
        ${_swarm_diligent_source_dir}/Graphics/GraphicsEngine/interface
)

target_include_directories(Swarm
    SYSTEM PRIVATE
        ${_swarm_diligent_source_dir}/Common/interface
        ${_swarm_diligent_source_dir}/Graphics/GraphicsEngineVulkan/interface
)

target_precompile_headers(Swarm
    PRIVATE
        include/swarm/precomp.h
)

target_link_libraries(Swarm
    PUBLIC
        Hive
        Comb
        Wax
        Terra
        Diligent-GraphicsEngineVk-static
        Diligent-GraphicsEngine
        Diligent-BuildSettings
)

if(UNIX AND TARGET Diligent-LinuxPlatform)
    # Static archive link order matters on ELF linkers. Repeat the platform
    # archive after the Diligent stack so DebugMessageCallback resolves.
    target_link_libraries(Swarm PUBLIC Diligent-LinuxPlatform)
endif()

else() # No render backend — null Swarm

hive_configure_target(Swarm)

target_sources(Swarm
    PRIVATE
        src/swarm.cpp
        src/swarmmodule.cpp
        src/platform/nullop/null_swarm.cpp
)

target_link_libraries(Swarm
    PUBLIC
        Hive
        Comb
        Wax
)

endif()
