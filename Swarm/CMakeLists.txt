# ============================================================================
# Swarm - RHI for HiveEngine
# ============================================================================

add_library(Swarm STATIC)

hive_configure_target(Swarm RTTI ON EXCEPTIONS ON)

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

set(_swarm_diligent_provider "${HIVE_DILIGENT_PROVIDER}")
if(NOT _swarm_diligent_provider)
    set(_swarm_diligent_provider "AUTO")
endif()
string(TOUPPER "${_swarm_diligent_provider}" _swarm_diligent_provider)

set(_swarm_diligent_package_dir "${HIVE_DILIGENT_PACKAGE_DIR}")
if(NOT _swarm_diligent_package_dir)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _swarm_diligent_config_suffix)
    if(NOT _swarm_diligent_config_suffix)
        set(_swarm_diligent_config_suffix "debug")
    endif()
    set(_swarm_diligent_package_dir
        "${CMAKE_SOURCE_DIR}/out/build/diligent-${_swarm_diligent_config_suffix}/package"
    )
endif()

set(_swarm_diligent_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/extern/diligentcore")
set(_swarm_using_prebuilt_diligent OFF)

if(_swarm_diligent_provider STREQUAL "AUTO" OR _swarm_diligent_provider STREQUAL "PREBUILT")
    set(_swarm_had_find_targets_global FALSE)
    if(DEFINED CMAKE_FIND_PACKAGE_TARGETS_GLOBAL)
        set(_swarm_had_find_targets_global TRUE)
        set(_swarm_prev_find_targets_global "${CMAKE_FIND_PACKAGE_TARGETS_GLOBAL}")
    endif()
    set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)
    find_package(HiveDiligent CONFIG QUIET
        PATHS "${_swarm_diligent_package_dir}"
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
        set(_swarm_using_prebuilt_diligent ON)
        if(HIVE_DILIGENT_SOURCE_DIR)
            set(_swarm_diligent_source_dir "${HIVE_DILIGENT_SOURCE_DIR}")
        endif()
        message(STATUS "Swarm: using prebuilt Diligent from ${_swarm_diligent_package_dir}")
    elseif(_swarm_diligent_provider STREQUAL "PREBUILT")
        message(FATAL_ERROR
            "Swarm: prebuilt Diligent was requested, but HiveDiligentConfig.cmake was not found in "
            "${_swarm_diligent_package_dir}. Run tools/build_diligent.ps1 (Windows) or tools/build_diligent.sh (Linux) first, "
            "or switch HIVE_DILIGENT_PROVIDER to AUTO/SOURCE."
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
        src/diligent_swarm.cpp
		src/diligent_swarm_vulkan.cpp
)

if(UNIX)
	target_sources(Swarm PRIVATE src/diligent_swarm_linux_vulkan.cpp)
endif ()

if(WIN32)
	target_sources(Swarm PRIVATE src/diligent_swarm_win32_vulkan.cpp)
endif ()

target_include_directories(Swarm
        PUBLIC
        include
)

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
		Diligent-GraphicsEngineVk-static
		Diligent-GraphicsEngine
		Diligent-BuildSettings
)
