# ============================================================================
# Swarm - RHI for HiveEngine
# ============================================================================

add_library(Swarm STATIC)

hive_configure_target(Swarm RTTI ON EXCEPTIONS ON)

# DiligentCore needs exceptions enabled and relaxed warnings.
# Only build the backends we actually use
# Auto-detect Windows SDK version for DiligentCore (works for any machine)
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
set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/diligentcore SYSTEM)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND TARGET Diligent-BuildSettings)
    # Diligent exports its own warning/optimization policy through an interface
    # target. Keep its compile definitions/includes, but let the engine own the
    # warning profile of first-party TUs.
    set_target_properties(Diligent-BuildSettings PROPERTIES INTERFACE_COMPILE_OPTIONS "")
endif()

# Re-enable exceptions and suppress warnings for all Diligent targets
function(swarm_fix_diligent_targets DIR)
    get_property(TARGETS DIRECTORY "${DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(TGT ${TARGETS})
        get_target_property(TGT_TYPE ${TGT} TYPE)
        get_target_property(TGT_IMPORTED ${TGT} IMPORTED)
        if(TGT_TYPE STREQUAL "INTERFACE_LIBRARY" OR TGT_TYPE STREQUAL "UTILITY" OR TGT_IMPORTED)
            continue()
        endif()
        if(MSVC)
            target_compile_options(${TGT} PRIVATE /EHsc /GR /w)
        else()
            target_compile_options(${TGT} PRIVATE -fexceptions -frtti -w)
        endif()
    endforeach()
    get_property(SUBDIRS DIRECTORY "${DIR}" PROPERTY SUBDIRECTORIES)
    foreach(SUBDIR ${SUBDIRS})
        swarm_fix_diligent_targets("${SUBDIR}")
    endforeach()
endfunction()

swarm_fix_diligent_targets("${CMAKE_CURRENT_SOURCE_DIR}/extern/diligentcore")

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
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/diligentcore/Graphics/GraphicsEngine/interface
)

target_include_directories(Swarm
        SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/diligentcore/Common/interface
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/diligentcore/Graphics/GraphicsEngineVulkan/interface
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

target_compile_features(Swarm
        PUBLIC
        cxx_std_20
)
