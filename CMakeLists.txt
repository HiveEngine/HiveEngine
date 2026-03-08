cmake_minimum_required(VERSION 3.28.3)

# Default to the bundled LLVM toolchain on Windows unless the user already
# selected a compiler or an explicit toolchain.
if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/llvm-windows.cmake"
        CACHE FILEPATH "Default Windows toolchain")
endif()

# Workaround for CMake 3.31 introspection bug with custom Clang builds on Windows
# Skip linking during try_compile to avoid malformed generator expressions
if(WIN32)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
endif()

# Clang on Windows needs RC compiler set before project()
# We don't use .rc files, but CMake requires it
if(WIN32 AND NOT CMAKE_RC_COMPILER)
    # Try to find rc.exe from Windows SDK (installed with MSVC)
    file(GLOB RC_PATHS
            "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/rc.exe"
            "C:/Program Files (x86)/Windows Kits/*/bin/x64/rc.exe"
    )
    if(RC_PATHS)
        list(GET RC_PATHS 0 FOUND_RC)
        set(CMAKE_RC_COMPILER "${FOUND_RC}" CACHE FILEPATH "RC Compiler")
    else()
        # Fallback: hope rc.exe is in PATH
        set(CMAKE_RC_COMPILER "rc" CACHE FILEPATH "RC Compiler")
    endif()
endif()

project(HiveEngine_Redo LANGUAGES CXX)

# ============================================================================
# Thread Configuration (must be before any subdirectory that uses Threads)
# ============================================================================
# On Windows, use native Win32 threads instead of pthreads
# This fixes linking issues with Clang on Windows trying to link pthreads.lib
if(WIN32)
    set(THREADS_PREFER_PTHREAD_FLAG OFF CACHE BOOL "" FORCE)
    set(CMAKE_USE_WIN32_THREADS_INIT ON CACHE BOOL "" FORCE)
    set(CMAKE_USE_PTHREADS_INIT OFF CACHE BOOL "" FORCE)
    set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "" FORCE)

    # Pre-create Threads::Threads target for Windows to prevent GLFW from
    # adding pthreads dependency. Windows has native thread support.
    if(NOT TARGET Threads::Threads)
        add_library(Threads::Threads INTERFACE IMPORTED)
        set(Threads_FOUND TRUE CACHE BOOL "" FORCE)
        set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "" FORCE)
    endif()
endif()

# ============================================================================
# Build Configuration
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Profile;Retail" CACHE STRING "" FORCE)

# ============================================================================
# Engine Mode
# ============================================================================
# HIVE_ENGINE_MODE: Editor, Game, or Headless
set(HIVE_ENGINE_MODE "Game" CACHE STRING "Engine mode: Editor, Game, or Headless")
set_property(CACHE HIVE_ENGINE_MODE PROPERTY STRINGS "Editor" "Game" "Headless")

# ============================================================================
# Feature Toggles
# ============================================================================
# These can be overridden individually. Headless mode is the only mode that
# forcibly disables graphics; other modes preserve explicit preset/user values.
set(HIVE_DEFAULT_FEATURE_IMGUI OFF)
set(HIVE_DEFAULT_FEATURE_VULKAN ON)
set(HIVE_DEFAULT_FEATURE_D3D12 OFF)
set(HIVE_DEFAULT_FEATURE_GLFW ON)

option(HIVE_FEATURE_IMGUI "Enable ImGui integration" ${HIVE_DEFAULT_FEATURE_IMGUI})
option(HIVE_FEATURE_VULKAN "Enable Vulkan renderer" ${HIVE_DEFAULT_FEATURE_VULKAN})
option(HIVE_FEATURE_D3D12 "Enable D3D12 renderer (Windows only)" ${HIVE_DEFAULT_FEATURE_D3D12})
option(HIVE_FEATURE_GLFW "Enable GLFW windowing" ${HIVE_DEFAULT_FEATURE_GLFW})

# Headless mode forces graphics off.
if(HIVE_ENGINE_MODE STREQUAL "Headless")
    set(HIVE_FEATURE_VULKAN OFF CACHE BOOL "Enable Vulkan renderer" FORCE)
    set(HIVE_FEATURE_D3D12 OFF CACHE BOOL "Enable D3D12 renderer" FORCE)
    set(HIVE_FEATURE_GLFW OFF CACHE BOOL "Enable GLFW windowing" FORCE)
    set(HIVE_FEATURE_IMGUI OFF CACHE BOOL "Enable ImGui integration" FORCE)
endif()

set(HIVE_HAS_RENDER_BACKEND OFF)
if(HIVE_FEATURE_VULKAN OR HIVE_FEATURE_D3D12)
    set(HIVE_HAS_RENDER_BACKEND ON)
endif()

set(HIVE_HAS_WINDOW_SYSTEM OFF)
if(HIVE_FEATURE_GLFW)
    set(HIVE_HAS_WINDOW_SYSTEM ON)
endif()

set(HIVE_HAS_INTERACTIVE_GRAPHICS OFF)
if(HIVE_HAS_RENDER_BACKEND AND HIVE_HAS_WINDOW_SYSTEM)
    set(HIVE_HAS_INTERACTIVE_GRAPHICS ON)
endif()

# Mutual exclusion: Vulkan and D3D12
if(HIVE_FEATURE_VULKAN AND HIVE_FEATURE_D3D12)
    message(FATAL_ERROR "Cannot enable both HIVE_FEATURE_VULKAN and HIVE_FEATURE_D3D12")
endif()

# D3D12 is Windows-only
if(HIVE_FEATURE_D3D12 AND NOT WIN32)
    message(FATAL_ERROR "HIVE_FEATURE_D3D12 is only supported on Windows")
endif()
option(HIVE_FEATURE_MEM_DEBUG "Enable memory debugging" OFF)
option(HIVE_FEATURE_PROFILER "Enable profiler" OFF)
option(HIVE_FEATURE_LOGGING "Enable logging" ON)
option(HIVE_FEATURE_ASSERTS "Enable asserts" ON)
option(HIVE_FEATURE_HOT_RELOAD "Enable hot reload" OFF)
option(HIVE_FEATURE_CONSOLE "Enable in-game console" ON)

# ============================================================================
# Engine Options
# ============================================================================

option(HIVE_ENABLE_LTO "Enable Link-Time Optimization (slower compile, better runtime)" ON)
option(HIVE_ENABLE_FAST_MATH "Enable fast math optimizations (less precise)" OFF)
option(HIVE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(HIVE_ENABLE_SIMD "Enable SIMD optimizations (SSE2)" ON)
option(HIVE_ENABLE_COMPILER_CACHE "Enable sccache/ccache when available" ON)
option(HIVE_BUILD_TESTING "Build tests and benchmarks" OFF)
set(HIVE_COMPILER_CACHE_PROGRAM "" CACHE FILEPATH "Explicit compiler cache launcher path")
set(HIVE_DILIGENT_PROVIDER "AUTO" CACHE STRING "How Swarm resolves Diligent: AUTO, PREBUILT, or SOURCE")
set_property(CACHE HIVE_DILIGENT_PROVIDER PROPERTY STRINGS AUTO PREBUILT SOURCE)
set(HIVE_DILIGENT_PACKAGE_DIR "" CACHE PATH "Path to a local HiveDiligent package directory")

# Sanitizer options (for Debug builds with Clang)
option(HIVE_ENABLE_SANITIZERS "Enable sanitizers (requires Clang)" OFF)
set(HIVE_SANITIZER_TYPE "Address" CACHE STRING "Sanitizer type: Address, UndefinedBehavior, Thread, Memory")
set_property(CACHE HIVE_SANITIZER_TYPE PROPERTY STRINGS "Address" "UndefinedBehavior" "Thread" "Memory")

# Clang sanitizer runtimes on Windows are built with static CRT (/MT)
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND HIVE_ENABLE_SANITIZERS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/HiveToolchain.cmake)

set(HIVE_COMPILER_CACHE_STATUS "OFF")
if(CMAKE_CXX_COMPILER_LAUNCHER)
    set(HIVE_COMPILER_CACHE_STATUS "${CMAKE_CXX_COMPILER_LAUNCHER} (preconfigured)")
elseif(HIVE_ENABLE_COMPILER_CACHE)
    set(_hive_compiler_cache_program "")
    if(HIVE_COMPILER_CACHE_PROGRAM)
        set(_hive_compiler_cache_program "${HIVE_COMPILER_CACHE_PROGRAM}")
    else()
        find_program(_hive_sccache_program NAMES sccache)
        find_program(_hive_ccache_program NAMES ccache)
        if(_hive_sccache_program)
            set(_hive_compiler_cache_program "${_hive_sccache_program}")
        elseif(_hive_ccache_program)
            set(_hive_compiler_cache_program "${_hive_ccache_program}")
        endif()
    endif()

    if(_hive_compiler_cache_program)
        set(CMAKE_C_COMPILER_LAUNCHER "${_hive_compiler_cache_program}" CACHE STRING "C compiler launcher" FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${_hive_compiler_cache_program}" CACHE STRING "CXX compiler launcher" FORCE)
        set(HIVE_COMPILER_CACHE_STATUS "${_hive_compiler_cache_program}")
    endif()
endif()

string(TOUPPER "${HIVE_DILIGENT_PROVIDER}" HIVE_DILIGENT_PROVIDER)
set(HIVE_DILIGENT_STATUS "${HIVE_DILIGENT_PROVIDER}")
if(HIVE_DILIGENT_PACKAGE_DIR)
    set(HIVE_DILIGENT_STATUS "${HIVE_DILIGENT_STATUS} @ ${HIVE_DILIGENT_PACKAGE_DIR}")
endif()

# Feature toggles — always define so C++ never falls back to HiveConfig.h guesses
macro(hive_define_feature name)
    if(${name})
        target_compile_definitions(HiveDefines_Features INTERFACE ${name}=1)
    else()
        target_compile_definitions(HiveDefines_Features INTERFACE ${name}=0)
    endif()
endmacro()

hive_define_feature(HIVE_FEATURE_IMGUI)
hive_define_feature(HIVE_FEATURE_VULKAN)
hive_define_feature(HIVE_FEATURE_D3D12)
hive_define_feature(HIVE_FEATURE_GLFW)
hive_define_feature(HIVE_FEATURE_MEM_DEBUG)
hive_define_feature(HIVE_FEATURE_PROFILER)
hive_define_feature(HIVE_FEATURE_LOGGING)
hive_define_feature(HIVE_FEATURE_ASSERTS)
hive_define_feature(HIVE_FEATURE_HOT_RELOAD)
hive_define_feature(HIVE_FEATURE_CONSOLE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
endforeach()

add_subdirectory(Hive)
if(HIVE_FEATURE_GLFW)
    add_subdirectory(Terra)
endif()
add_subdirectory(Comb)
add_subdirectory(Wax)
add_subdirectory(Nectar)
add_subdirectory(Queen)
add_subdirectory(Waggle)
if(HIVE_FEATURE_GLFW)
    add_subdirectory(Antennae)
endif()
if(HIVE_HAS_RENDER_BACKEND)
    add_subdirectory(Swarm)
endif()
if(HIVE_ENGINE_MODE STREQUAL "Editor" AND HIVE_FEATURE_IMGUI AND HIVE_HAS_INTERACTIVE_GRAPHICS)
    add_subdirectory(Forge)
endif()
if(HIVE_BUILD_TESTING)
    add_subdirectory(Larvae)
endif()
add_subdirectory(Brood)
if(HIVE_HAS_INTERACTIVE_GRAPHICS)
    add_subdirectory(Testbed)
endif()
add_subdirectory(projects)

message(STATUS "")
message(STATUS "====================================")
message(STATUS "  HiveEngine Build Configuration")
message(STATUS "====================================")
message(STATUS "Build type:         ${CMAKE_BUILD_TYPE}")
message(STATUS "Engine mode:        ${HIVE_ENGINE_MODE}")
message(STATUS "C++ Standard:       ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:           ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  ImGui:            ${HIVE_FEATURE_IMGUI}")
message(STATUS "  Vulkan:           ${HIVE_FEATURE_VULKAN}")
message(STATUS "  D3D12:            ${HIVE_FEATURE_D3D12}")
message(STATUS "  GLFW:             ${HIVE_FEATURE_GLFW}")
message(STATUS "  MemDebug:         ${HIVE_FEATURE_MEM_DEBUG}")
message(STATUS "  Profiler:         ${HIVE_FEATURE_PROFILER}")
message(STATUS "  Logging:          ${HIVE_FEATURE_LOGGING}")
message(STATUS "  Asserts:          ${HIVE_FEATURE_ASSERTS}")
message(STATUS "  HotReload:        ${HIVE_FEATURE_HOT_RELOAD}")
message(STATUS "  Console:          ${HIVE_FEATURE_CONSOLE}")
message(STATUS "")
message(STATUS "Optimizations:")
message(STATUS "  RTTI:             OFF")
message(STATUS "  Exceptions:       OFF")
message(STATUS "  LTO:              ${HIVE_ENABLE_LTO}")
message(STATUS "  Fast Math:        ${HIVE_ENABLE_FAST_MATH}")
message(STATUS "  SIMD:             ${HIVE_ENABLE_SIMD}")
message(STATUS "  Diligent:         ${HIVE_DILIGENT_STATUS}")
message(STATUS "  Compiler Cache:   ${HIVE_COMPILER_CACHE_STATUS}")
message(STATUS "  Testing:          ${HIVE_BUILD_TESTING}")
message(STATUS "  Warnings->Errors: ${HIVE_WARNINGS_AS_ERRORS}")
if(HIVE_ENABLE_SANITIZERS)
    message(STATUS "  Sanitizers:       ${HIVE_SANITIZER_TYPE}")
endif()
message(STATUS "")
message(STATUS "Output:")
message(STATUS "  Binaries:         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  Libraries:        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "====================================")
message(STATUS "")
