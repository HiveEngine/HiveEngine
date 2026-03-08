cmake_minimum_required(VERSION 3.28.3)

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

include(CMakeParseArguments)

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
# These can be overridden individually. Default values depend on mode/config.

option(HIVE_FEATURE_IMGUI "Enable ImGui integration" OFF)
option(HIVE_FEATURE_VULKAN "Enable Vulkan renderer" ON)
option(HIVE_FEATURE_D3D12 "Enable D3D12 renderer (Windows only)" OFF)
option(HIVE_FEATURE_GLFW "Enable GLFW windowing" ON)

# Headless mode forces graphics off; non-headless restores defaults
if(HIVE_ENGINE_MODE STREQUAL "Headless")
    set(HIVE_FEATURE_VULKAN OFF CACHE BOOL "Enable Vulkan renderer" FORCE)
    set(HIVE_FEATURE_D3D12 OFF CACHE BOOL "Enable D3D12 renderer" FORCE)
    set(HIVE_FEATURE_GLFW OFF CACHE BOOL "Enable GLFW windowing" FORCE)
    set(HIVE_FEATURE_IMGUI OFF CACHE BOOL "Enable ImGui integration" FORCE)
elseif(NOT HIVE_FEATURE_D3D12)
    set(HIVE_FEATURE_VULKAN ON CACHE BOOL "Enable Vulkan renderer" FORCE)
    set(HIVE_FEATURE_GLFW ON CACHE BOOL "Enable GLFW windowing" FORCE)
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

# Sanitizer options (for Debug builds with Clang)
option(HIVE_ENABLE_SANITIZERS "Enable sanitizers (requires Clang)" OFF)
set(HIVE_SANITIZER_TYPE "Address" CACHE STRING "Sanitizer type: Address, UndefinedBehavior, Thread, Memory")
set_property(CACHE HIVE_SANITIZER_TYPE PROPERTY STRINGS "Address" "UndefinedBehavior" "Thread" "Memory")

# Clang sanitizer runtimes on Windows are built with static CRT (/MT)
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND HIVE_ENABLE_SANITIZERS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)
endif()

# Legacy compatibility
set(swarm_use_vulkan ${HIVE_FEATURE_VULKAN} CACHE BOOL "Enable Vulkan GPU allocator" FORCE)
set(terra_use_glfw ${HIVE_FEATURE_GLFW} CACHE BOOL "Enable GLFW windowing" FORCE)

# ============================================================================
# Toolchain Policy
# ============================================================================

add_library(HiveToolchain_Base INTERFACE)
add_library(HiveToolchain_Warnings INTERFACE)
add_library(HiveToolchain_Config INTERFACE)
add_library(HiveToolchain_FastMath INTERFACE)
add_library(HiveToolchain_SIMD INTERFACE)
add_library(HiveToolchain_Sanitizers INTERFACE)
add_library(HiveToolchain_LTO INTERFACE)
add_library(HiveDefines_Platform INTERFACE)
add_library(HiveDefines_Config INTERFACE)
add_library(HiveDefines_Mode INTERFACE)

add_library(HiveFeature_Exception_On INTERFACE)
add_library(HiveFeature_Exception_Off INTERFACE)
add_library(HiveFeature_RTTI_On INTERFACE)
add_library(HiveFeature_RTTI_Off INTERFACE)
add_library(HiveFeature_WError INTERFACE)

set(HIVE_COMPILER_IS_CLANG_CL OFF)
if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(HIVE_COMPILER_IS_CLANG_CL ON)
endif()

if(MSVC)
    target_compile_options(HiveToolchain_Base INTERFACE /permissive- /Zc:__cplusplus /Zc:inline /utf-8)
    if(NOT HIVE_COMPILER_IS_CLANG_CL)
        target_compile_options(HiveToolchain_Base INTERFACE /MP /external:W0)
    endif()

    target_compile_options(HiveToolchain_Warnings INTERFACE
        /W4
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /w14296
        /w14640
        /wd4100
        /wd4530
    )

    if(HIVE_COMPILER_IS_CLANG_CL)
        target_compile_options(HiveToolchain_Warnings INTERFACE
            -Wno-unsafe-buffer-usage
            -Wno-unsafe-buffer-usage-in-libc-call
            -Wno-c++98-compat
        )
    endif()

    target_compile_options(HiveToolchain_Config INTERFACE
        "$<$<CONFIG:Debug>:/Od;/MDd;/Zi>"
        "$<$<CONFIG:Release>:/O2;/Oi;/MD;/GS-;/Gy>"
        "$<$<CONFIG:RelWithDebInfo>:/O2;/Zi;/MD>"
        "$<$<CONFIG:Profile>:/O2;/Zi;/MD;/GS-;/Gy>"
        "$<$<CONFIG:Retail>:/O2;/Oi;/MD;/GS-;/Gy>"
    )

    target_compile_definitions(HiveToolchain_Config INTERFACE
        "$<$<CONFIG:Debug>:_DEBUG;_ITERATOR_DEBUG_LEVEL=2>"
        "$<$<CONFIG:Release>:NDEBUG;_ITERATOR_DEBUG_LEVEL=0>"
        "$<$<CONFIG:RelWithDebInfo>:NDEBUG;_ITERATOR_DEBUG_LEVEL=1>"
        "$<$<CONFIG:Profile>:NDEBUG;_ITERATOR_DEBUG_LEVEL=0>"
        "$<$<CONFIG:Retail>:NDEBUG;_ITERATOR_DEBUG_LEVEL=0>"
    )

    target_compile_options(HiveFeature_Exception_Off INTERFACE /EHs-c-)
    target_compile_options(HiveFeature_Exception_On INTERFACE /EHsc)
    target_compile_options(HiveFeature_RTTI_Off INTERFACE /GR-)
    target_compile_options(HiveFeature_RTTI_On INTERFACE /GR)
    target_compile_options(HiveFeature_WError INTERFACE /WX)

    if(HIVE_ENABLE_FAST_MATH)
        target_compile_options(HiveToolchain_FastMath INTERFACE /fp:fast)
    endif()

    if(HIVE_ENABLE_SIMD AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_options(HiveToolchain_SIMD INTERFACE /arch:SSE2)
    endif()
else()
    target_compile_options(HiveToolchain_Warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
    )

    target_compile_options(HiveToolchain_Config INTERFACE
        "$<$<CONFIG:Debug>:-O0;-g3>"
        "$<$<CONFIG:Release>:-O3;-ffunction-sections;-fdata-sections>"
        "$<$<CONFIG:RelWithDebInfo>:-O2;-g>"
        "$<$<CONFIG:Profile>:-O2;-g;-fno-omit-frame-pointer>"
        "$<$<CONFIG:Retail>:-O3;-ffunction-sections;-fdata-sections>"
    )

    target_compile_definitions(HiveToolchain_Config INTERFACE
        "$<$<CONFIG:Debug>:_GLIBCXX_DEBUG>"
    )

    if(NOT WIN32)
        target_link_options(HiveToolchain_Config INTERFACE
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:Retail>>:-Wl,--gc-sections>"
        )
    endif()

    target_compile_options(HiveFeature_Exception_Off INTERFACE -fno-exceptions)
    target_compile_options(HiveFeature_Exception_On INTERFACE -fexceptions)
    target_compile_options(HiveFeature_RTTI_Off INTERFACE -fno-rtti)
    target_compile_options(HiveFeature_RTTI_On INTERFACE -frtti)
    target_compile_options(HiveFeature_WError INTERFACE -Werror)

    if(HIVE_ENABLE_FAST_MATH)
        target_compile_options(HiveToolchain_FastMath INTERFACE -ffast-math)
    endif()

    if(HIVE_ENABLE_SIMD)
        target_compile_options(HiveToolchain_SIMD INTERFACE -msse2)
    endif()

    if(HIVE_ENABLE_SANITIZERS)
        target_compile_options(HiveToolchain_Sanitizers INTERFACE
            "$<$<CONFIG:Debug>:-fno-omit-frame-pointer;-fno-optimize-sibling-calls>"
        )

        if(HIVE_SANITIZER_TYPE STREQUAL "Address")
            target_compile_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=address>")
            target_link_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=address>")
            message(STATUS "AddressSanitizer enabled for Debug builds")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "UndefinedBehavior")
            if(WIN32)
                target_compile_options(HiveToolchain_Sanitizers INTERFACE
                    "$<$<CONFIG:Debug>:-fsanitize=undefined;-fno-sanitize=alignment,function,vptr>"
                )
                target_link_options(HiveToolchain_Sanitizers INTERFACE
                    "$<$<CONFIG:Debug>:-fsanitize=undefined;-fno-sanitize=alignment,function,vptr>"
                )
            else()
                target_compile_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=undefined>")
                target_link_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=undefined>")
            endif()
            message(STATUS "UndefinedBehaviorSanitizer enabled for Debug builds")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "Thread")
            target_compile_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=thread>")
            target_link_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=thread>")
            message(STATUS "ThreadSanitizer enabled for Debug builds")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "Memory")
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
                target_compile_options(HiveToolchain_Sanitizers INTERFACE
                    "$<$<CONFIG:Debug>:-fsanitize=memory;-fsanitize-memory-track-origins>"
                )
                target_link_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=memory>")
                message(STATUS "MemorySanitizer enabled for Debug builds")
            else()
                message(WARNING "MemorySanitizer only available with Clang on Linux/macOS")
            endif()
        endif()
    endif()
endif()

set(HIVE_IPO_SUPPORTED OFF)
if(HIVE_ENABLE_LTO)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
        target_compile_options(HiveToolchain_LTO INTERFACE
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Profile>,$<CONFIG:Retail>>:-flto=thin>"
        )
        target_link_options(HiveToolchain_LTO INTERFACE
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Profile>,$<CONFIG:Retail>>:-flto=thin>"
        )
        message(STATUS "LTO enabled for optimized builds (Clang thin LTO)")
    else()
        include(CheckIPOSupported)
        check_ipo_supported(RESULT HIVE_IPO_SUPPORTED OUTPUT HIVE_IPO_OUTPUT)
        if(HIVE_IPO_SUPPORTED)
            message(STATUS "LTO/IPO enabled for optimized builds")
        else()
            message(WARNING "LTO/IPO not supported: ${HIVE_IPO_OUTPUT}")
        endif()
    endif()
endif()

function(hive_get_target_scope out_var target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Unknown target '${target}'")
    endif()

    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
        set(${out_var} INTERFACE PARENT_SCOPE)
    else()
        set(${out_var} PRIVATE PARENT_SCOPE)
    endif()
endfunction()

function(hive_configure_target target)
    set(options NO_WARNINGS)
    set(oneValueArgs RTTI EXCEPTIONS)
    cmake_parse_arguments(HIVE_CFG "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT HIVE_CFG_RTTI)
        set(HIVE_CFG_RTTI OFF)
    endif()

    if(NOT HIVE_CFG_EXCEPTIONS)
        set(HIVE_CFG_EXCEPTIONS OFF)
    endif()

    hive_get_target_scope(_scope ${target})

    target_link_libraries(${target} ${_scope}
        HiveToolchain_Base
        HiveToolchain_Config
        HiveDefines_Platform
        HiveDefines_Config
        HiveDefines_Mode
    )

    if(NOT HIVE_CFG_NO_WARNINGS)
        target_link_libraries(${target} ${_scope} HiveToolchain_Warnings)
        if(HIVE_WARNINGS_AS_ERRORS)
            target_link_libraries(${target} ${_scope} HiveFeature_WError)
        endif()
    endif()

    if(HIVE_ENABLE_FAST_MATH)
        target_link_libraries(${target} ${_scope} HiveToolchain_FastMath)
    endif()

    if(HIVE_ENABLE_SIMD)
        target_link_libraries(${target} ${_scope} HiveToolchain_SIMD)
    endif()

    if(HIVE_ENABLE_SANITIZERS)
        target_link_libraries(${target} ${_scope} HiveToolchain_Sanitizers)
    endif()

    if(HIVE_ENABLE_LTO AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
        target_link_libraries(${target} ${_scope} HiveToolchain_LTO)
    endif()

    if(HIVE_CFG_EXCEPTIONS STREQUAL "ON")
        target_link_libraries(${target} ${_scope} HiveFeature_Exception_On)
    elseif(HIVE_CFG_EXCEPTIONS STREQUAL "OFF")
        target_link_libraries(${target} ${_scope} HiveFeature_Exception_Off)
    else()
        message(FATAL_ERROR "hive_configure_target(${target}): EXCEPTIONS must be ON or OFF")
    endif()

    if(HIVE_CFG_RTTI STREQUAL "ON")
        target_link_libraries(${target} ${_scope} HiveFeature_RTTI_On)
    elseif(HIVE_CFG_RTTI STREQUAL "OFF")
        target_link_libraries(${target} ${_scope} HiveFeature_RTTI_Off)
    else()
        message(FATAL_ERROR "hive_configure_target(${target}): RTTI must be ON or OFF")
    endif()

    get_target_property(_type ${target} TYPE)
    if(HIVE_ENABLE_LTO AND HIVE_IPO_SUPPORTED AND NOT _type STREQUAL "INTERFACE_LIBRARY")
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_PROFILE ON)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RETAIL ON)
    endif()
endfunction()

# Platform detection
if(WIN32)
    target_compile_definitions(HiveDefines_Platform INTERFACE HIVE_PLATFORM_WINDOWS=1)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(HiveDefines_Platform INTERFACE HIVE_PLATFORM_LINUX=1)
elseif(APPLE)
    target_compile_definitions(HiveDefines_Platform INTERFACE HIVE_PLATFORM_MACOS=1)
endif()

# ============================================================================
# HiveConfig.h Definitions (passed to compiler)
# ============================================================================

# Build configuration
target_compile_definitions(HiveDefines_Config INTERFACE
    $<$<CONFIG:Debug>:HIVE_CONFIG_DEBUG=1>
    $<$<CONFIG:Release>:HIVE_CONFIG_RELEASE=1>
    $<$<CONFIG:RelWithDebInfo>:HIVE_CONFIG_RELEASE=1>
    $<$<CONFIG:Profile>:HIVE_CONFIG_PROFILE=1>
    $<$<CONFIG:Retail>:HIVE_CONFIG_RETAIL=1>
)

# Engine mode
if(HIVE_ENGINE_MODE STREQUAL "Editor")
    target_compile_definitions(HiveDefines_Mode INTERFACE HIVE_MODE_EDITOR=1)
elseif(HIVE_ENGINE_MODE STREQUAL "Game")
    target_compile_definitions(HiveDefines_Mode INTERFACE HIVE_MODE_GAME=1)
elseif(HIVE_ENGINE_MODE STREQUAL "Headless")
    target_compile_definitions(HiveDefines_Mode INTERFACE HIVE_MODE_HEADLESS=1)
endif()

# Feature toggles — always define so C++ never falls back to HiveConfig.h guesses
macro(hive_define_feature name)
    if(${name})
        add_compile_definitions(${name}=1)
    else()
        add_compile_definitions(${name}=0)
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
add_subdirectory(Larvae)
add_subdirectory(Comb)
add_subdirectory(Wax)
add_subdirectory(Nectar)
add_subdirectory(Queen)
add_subdirectory(Waggle)
if(HIVE_FEATURE_GLFW)
    add_subdirectory(Antennae)
endif()
add_subdirectory(Swarm)
if(HIVE_ENGINE_MODE STREQUAL "Editor")
    add_subdirectory(Forge)
endif()
add_subdirectory(Brood)
add_subdirectory(Testbed)
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
