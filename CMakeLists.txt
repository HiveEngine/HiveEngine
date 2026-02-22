cmake_minimum_required(VERSION 3.30.5)

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
# These can be overridden individually. Default values depend on mode/config.

option(HIVE_FEATURE_IMGUI "Enable ImGui integration" OFF)
option(HIVE_FEATURE_VULKAN "Enable Vulkan renderer" ON)
option(HIVE_FEATURE_GLFW "Enable GLFW windowing" ON)
option(HIVE_FEATURE_MEM_DEBUG "Enable memory debugging" OFF)
option(HIVE_FEATURE_PROFILER "Enable profiler" ON)
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
# Compiler Flags - ALL PLATFORMS
# ============================================================================

# Disable RTTI (Run-Time Type Information)
# Saves 15-20% binary size, improves performance
#if(MSVC)
#    add_compile_options(/GR-)
#else()
#    add_compile_options(-fno-rtti)
#endif()
#
## Disable C++ Exceptions
## Saves 5-10% binary size, improves performance
## WARNING: STL will call std::terminate on errors
#if(MSVC)
#    # Remove CMake's default /EHsc flag to avoid D9025 warning
#    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#    add_compile_options(/EHs-c-)
#else()
#    add_compile_options(-fno-exceptions)
#endif()

# ============================================================================
# Compiler Flags
# ============================================================================

# MSVC (not Clang-CL)
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
            /MP /permissive- /Zc:__cplusplus /Zc:inline /utf-8
    )

    add_compile_options(/external:W0 /external:anglebrackets)

    add_compile_options(
            /W4                  # Warning level 4
            /w14242              # Conversion, possible loss of data
            /w14254              # Operator conversion, possible loss of data
            /w14263              # Member function does not override
            /w14265              # Class has virtual functions but no virtual destructor
            /w14287              # Unsigned/negative constant mismatch
            /w14296              # Expression is always constant
            /w14640              # Thread-unsafe static member initialization
            /wd4100              # Disable: unreferenced formal parameter (too noisy)
            /wd4530              # Disable: C++ exception handler (from external headers with /EHs-c-)
    )

    if(HIVE_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()

    # Debug flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Od /MDd /Zi)
        add_compile_definitions(_DEBUG _ITERATOR_DEBUG_LEVEL=2)
    endif()

    # Release flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /Oi /MD /GS- /Gy)
        add_compile_definitions(NDEBUG _ITERATOR_DEBUG_LEVEL=0)

        if(HIVE_ENABLE_LTO)
            add_compile_options(/GL)
            add_link_options(/LTCG /OPT:REF /OPT:ICF)
        endif()

        if(HIVE_ENABLE_FAST_MATH)
            add_compile_options(/fp:fast)
        endif()
    endif()

    # RelWithDebInfo flags
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(/O2 /Zi /MD)
        add_compile_definitions(NDEBUG _ITERATOR_DEBUG_LEVEL=1)
    endif()

    if(HIVE_ENABLE_SIMD)
        add_compile_options(/arch:SSE2)
    endif()

endif()

# GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
            -Wall                    # Enable most warnings
            -Wextra                  # Extra warnings
            -Wpedantic               # Strict ISO C++
            -Wshadow                 # Warn if variable shadows
            -Wnon-virtual-dtor       # Virtual functions without virtual destructor
            -Wold-style-cast         # C-style casts
            -Wcast-align             # Unaligned pointer casts
            -Wunused                 # Unused variables/functions
            -Woverloaded-virtual     # Missing override
            -Wconversion             # Implicit type conversions
            -Wsign-conversion        # Sign conversions
            -Wdouble-promotion       # float->double promotions
    )

    if(HIVE_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()

    # Debug flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g3)
#        add_compile_definitions(_GLIBCXX_DEBUG)
    endif()

    # Release flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -ffunction-sections -fdata-sections)

        # Use --gc-sections only on Linux/macOS (not Windows where LLD doesn't support it)
        if(NOT WIN32)
            add_link_options(-Wl,--gc-sections)
        endif()

        if(HIVE_ENABLE_FAST_MATH)
            add_compile_options(-ffast-math)
        endif()
    endif()

    # RelWithDebInfo flags
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g)
    endif()

    # Profile flags (Release with profiling)
    if(CMAKE_BUILD_TYPE STREQUAL "Profile")
        add_compile_options(-O2 -g -fno-omit-frame-pointer)
    endif()

    # Retail flags (Maximum optimization, no debug)
    if(CMAKE_BUILD_TYPE STREQUAL "Retail")
        add_compile_options(-O3 -ffunction-sections -fdata-sections)
        if(NOT WIN32)
            add_link_options(-Wl,--gc-sections)
        endif()
        if(HIVE_ENABLE_FAST_MATH)
            add_compile_options(-ffast-math)
        endif()
    endif()

    # Sanitizers (available in Debug)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND HIVE_ENABLE_SANITIZERS)
        add_compile_options(-fno-omit-frame-pointer -fno-optimize-sibling-calls)

        if(HIVE_SANITIZER_TYPE STREQUAL "Address")
            add_compile_options(-fsanitize=address)
            add_link_options(-fsanitize=address)
            message(STATUS "AddressSanitizer enabled")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "UndefinedBehavior")
            if(WIN32)
                add_compile_options(-fsanitize=undefined -fno-sanitize=alignment,function,vptr)
                add_link_options(-fsanitize=undefined -fno-sanitize=alignment,function,vptr)
            else()
                add_compile_options(-fsanitize=undefined)
                add_link_options(-fsanitize=undefined)
            endif()
            message(STATUS "UndefinedBehaviorSanitizer enabled")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "Thread")
            add_compile_options(-fsanitize=thread)
            add_link_options(-fsanitize=thread)
            message(STATUS "ThreadSanitizer enabled")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "Memory")
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
                add_compile_options(-fsanitize=memory -fsanitize-memory-track-origins)
                add_link_options(-fsanitize=memory)
                message(STATUS "MemorySanitizer enabled")
            else()
                message(WARNING "MemorySanitizer only available with Clang on Linux/macOS")
            endif()
        endif()
    endif()

    if(HIVE_ENABLE_SIMD)
        add_compile_options(-msse2)
    endif()
endif()

if(HIVE_ENABLE_LTO AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    # For Clang on Windows, manually enable LTO via compile flags
    # check_ipo_supported() has issues with custom Clang builds
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
        add_compile_options($<$<CONFIG:Release>:-flto=thin>)
        add_compile_options($<$<CONFIG:RelWithDebInfo>:-flto=thin>)
        add_link_options($<$<CONFIG:Release>:-flto=thin>)
        add_link_options($<$<CONFIG:RelWithDebInfo>:-flto=thin>)
        message(STATUS "✅ LTO enabled for Release builds (Clang thin LTO)")
    else()
        include(CheckIPOSupported)
        check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)

        if(ipo_supported)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
            message(STATUS "✅ LTO/IPO enabled for Release builds")
        else()
            message(WARNING "⚠️ LTO/IPO not supported: ${ipo_output}")
        endif()
    endif()
endif()

# Platform detection
if(WIN32)
    add_compile_definitions(HIVE_PLATFORM_WINDOWS=1)
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(HIVE_PLATFORM_LINUX=1)
elseif(APPLE)
    add_compile_definitions(HIVE_PLATFORM_MACOS=1)
endif()

# ============================================================================
# HiveConfig.h Definitions (passed to compiler)
# ============================================================================

# Build configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(HIVE_CONFIG_DEBUG=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(HIVE_CONFIG_RELEASE=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Profile")
    add_compile_definitions(HIVE_CONFIG_PROFILE=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Retail")
    add_compile_definitions(HIVE_CONFIG_RETAIL=1)
else()
    # Default/RelWithDebInfo -> Release
    add_compile_definitions(HIVE_CONFIG_RELEASE=1)
endif()

# Engine mode
if(HIVE_ENGINE_MODE STREQUAL "Editor")
    add_compile_definitions(HIVE_MODE_EDITOR=1)
elseif(HIVE_ENGINE_MODE STREQUAL "Game")
    add_compile_definitions(HIVE_MODE_GAME=1)
elseif(HIVE_ENGINE_MODE STREQUAL "Headless")
    add_compile_definitions(HIVE_MODE_HEADLESS=1)
endif()

# Feature toggles (only define if explicitly set, otherwise use HiveConfig.h defaults)
if(HIVE_FEATURE_IMGUI)
    add_compile_definitions(HIVE_FEATURE_IMGUI=1)
endif()
if(HIVE_FEATURE_VULKAN)
    add_compile_definitions(HIVE_FEATURE_VULKAN=1)
endif()
if(HIVE_FEATURE_GLFW)
    add_compile_definitions(HIVE_FEATURE_GLFW=1)
endif()
if(HIVE_FEATURE_MEM_DEBUG)
    add_compile_definitions(HIVE_FEATURE_MEM_DEBUG=1)
endif()
if(HIVE_FEATURE_PROFILER)
    add_compile_definitions(HIVE_FEATURE_PROFILER=1)
endif()
if(HIVE_FEATURE_LOGGING)
    add_compile_definitions(HIVE_FEATURE_LOGGING=1)
endif()
if(HIVE_FEATURE_ASSERTS)
    add_compile_definitions(HIVE_FEATURE_ASSERTS=1)
endif()
if(HIVE_FEATURE_HOT_RELOAD)
    add_compile_definitions(HIVE_FEATURE_HOT_RELOAD=1)
endif()
if(HIVE_FEATURE_CONSOLE)
    add_compile_definitions(HIVE_FEATURE_CONSOLE=1)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
endforeach()

include_directories("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.43.34808/atlmfc/include")
link_directories("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.43.34808/atlmfc/lib/x64")

add_subdirectory(Hive)
add_subdirectory(Terra)
add_subdirectory(Larvae)
add_subdirectory(Comb)
add_subdirectory(Wax)
add_subdirectory(Nectar)
add_subdirectory(Queen)
add_subdirectory(Brood)
add_subdirectory(Swarm)
add_subdirectory(Testbed)
#add_subdirectory(Cells)

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
