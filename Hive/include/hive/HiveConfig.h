#pragma once

// ============================================================================
// HiveEngine Configuration Header
// ============================================================================

// ============================================================================
// Platform Detection (auto-detected)
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define HIVE_PLATFORM_WINDOWS 1
    #define HIVE_PLATFORM_LINUX   0
    #define HIVE_PLATFORM_MACOS   0
#elif defined(__linux__)
    #define HIVE_PLATFORM_WINDOWS 0
    #define HIVE_PLATFORM_LINUX   1
    #define HIVE_PLATFORM_MACOS   0
#elif defined(__APPLE__) && defined(__MACH__)
    #define HIVE_PLATFORM_WINDOWS 0
    #define HIVE_PLATFORM_LINUX   0
    #define HIVE_PLATFORM_MACOS   1
#else
    #define HIVE_PLATFORM_WINDOWS 0
    #define HIVE_PLATFORM_LINUX   0
    #define HIVE_PLATFORM_MACOS   0
    #define HIVE_PLATFORM_UNKNOWN 1
#endif

// ============================================================================
// Compiler Detection (auto-detected)
// ============================================================================

#if defined(_MSC_VER) && !defined(__clang__)
    #define HIVE_COMPILER_MSVC  1
    #define HIVE_COMPILER_CLANG 0
    #define HIVE_COMPILER_GCC   0
#elif defined(__clang__)
    #define HIVE_COMPILER_MSVC  0
    #define HIVE_COMPILER_CLANG 1
    #define HIVE_COMPILER_GCC   0
#elif defined(__GNUC__)
    #define HIVE_COMPILER_MSVC  0
    #define HIVE_COMPILER_CLANG 0
    #define HIVE_COMPILER_GCC   1
#else
    #define HIVE_COMPILER_MSVC  0
    #define HIVE_COMPILER_CLANG 0
    #define HIVE_COMPILER_GCC   0
#endif

// ============================================================================
// Build Configuration (set via CMake: -DHIVE_CONFIG_DEBUG=1, etc.)
// ============================================================================
// Only ONE of these should be 1.
//
// HIVE_CONFIG_DEBUG   - Development builds, all debugging enabled
// HIVE_CONFIG_RELEASE - Optimized builds with some debugging
// HIVE_CONFIG_PROFILE - Optimized builds with profiling enabled
// HIVE_CONFIG_RETAIL  - Final shipping builds, no debugging
// ============================================================================

#ifndef HIVE_CONFIG_DEBUG
    #define HIVE_CONFIG_DEBUG 0
#endif

#ifndef HIVE_CONFIG_RELEASE
    #define HIVE_CONFIG_RELEASE 0
#endif

#ifndef HIVE_CONFIG_PROFILE
    #define HIVE_CONFIG_PROFILE 0
#endif

#ifndef HIVE_CONFIG_RETAIL
    #define HIVE_CONFIG_RETAIL 0
#endif

// Default to Debug if nothing specified
#if (HIVE_CONFIG_DEBUG + HIVE_CONFIG_RELEASE + HIVE_CONFIG_PROFILE + HIVE_CONFIG_RETAIL) == 0
    #undef HIVE_CONFIG_DEBUG
    #define HIVE_CONFIG_DEBUG 1
#endif

// ============================================================================
// Engine Mode (set via CMake: -DHIVE_MODE_EDITOR=1, etc.)
// ============================================================================
// Only ONE of these should be 1.
//
// HIVE_MODE_EDITOR   - Editor mode with full tooling
// HIVE_MODE_GAME     - Game/runtime mode
// HIVE_MODE_HEADLESS - No graphics, server/CLI mode
// ============================================================================

#ifndef HIVE_MODE_EDITOR
    #define HIVE_MODE_EDITOR 0
#endif

#ifndef HIVE_MODE_GAME
    #define HIVE_MODE_GAME 0
#endif

#ifndef HIVE_MODE_HEADLESS
    #define HIVE_MODE_HEADLESS 0
#endif

// Default to Game mode if nothing specified
#if (HIVE_MODE_EDITOR + HIVE_MODE_GAME + HIVE_MODE_HEADLESS) == 0
    #undef HIVE_MODE_GAME
    #define HIVE_MODE_GAME 1
#endif

// ============================================================================
// Feature Toggles (set via CMake: -DHIVE_FEATURE_IMGUI=1, etc.)
// ============================================================================

// --- Graphics & UI ---

#ifndef HIVE_FEATURE_IMGUI
    #if HIVE_MODE_EDITOR && !HIVE_CONFIG_RETAIL
        #define HIVE_FEATURE_IMGUI 1
    #else
        #define HIVE_FEATURE_IMGUI 0
    #endif
#endif

#ifndef HIVE_FEATURE_VULKAN
    #if !HIVE_MODE_HEADLESS
        #define HIVE_FEATURE_VULKAN 1
    #else
        #define HIVE_FEATURE_VULKAN 0
    #endif
#endif

#ifndef HIVE_FEATURE_GLFW
    #if !HIVE_MODE_HEADLESS
        #define HIVE_FEATURE_GLFW 1
    #else
        #define HIVE_FEATURE_GLFW 0
    #endif
#endif

// --- Debugging & Profiling ---

#ifndef HIVE_FEATURE_MEM_DEBUG
    #if HIVE_CONFIG_DEBUG
        #define HIVE_FEATURE_MEM_DEBUG 1
    #else
        #define HIVE_FEATURE_MEM_DEBUG 0
    #endif
#endif

#ifndef HIVE_FEATURE_PROFILER
    #if !HIVE_CONFIG_RETAIL
        #define HIVE_FEATURE_PROFILER 1
    #else
        #define HIVE_FEATURE_PROFILER 0
    #endif
#endif

#ifndef HIVE_FEATURE_LOGGING
    #if !HIVE_CONFIG_RETAIL
        #define HIVE_FEATURE_LOGGING 1
    #else
        #define HIVE_FEATURE_LOGGING 0
    #endif
#endif

#ifndef HIVE_FEATURE_ASSERTS
    #if HIVE_CONFIG_DEBUG
        #define HIVE_FEATURE_ASSERTS 1
    #else
        #define HIVE_FEATURE_ASSERTS 0
    #endif
#endif

// --- Editor Features ---

#ifndef HIVE_FEATURE_HOT_RELOAD
    #if HIVE_MODE_EDITOR && !HIVE_CONFIG_RETAIL
        #define HIVE_FEATURE_HOT_RELOAD 1
    #else
        #define HIVE_FEATURE_HOT_RELOAD 0
    #endif
#endif

#ifndef HIVE_FEATURE_CONSOLE
    #if !HIVE_CONFIG_RETAIL
        #define HIVE_FEATURE_CONSOLE 1
    #else
        #define HIVE_FEATURE_CONSOLE 0
    #endif
#endif

// ============================================================================
// Log Level (0=Trace, 1=Debug, 2=Info, 3=Warning, 4=Error, 5=Fatal)
// ============================================================================

#ifndef HIVE_LOG_LEVEL
    #if HIVE_CONFIG_DEBUG
        #define HIVE_LOG_LEVEL 0
    #elif HIVE_CONFIG_RELEASE
        #define HIVE_LOG_LEVEL 2
    #elif HIVE_CONFIG_PROFILE
        #define HIVE_LOG_LEVEL 3
    #else
        #define HIVE_LOG_LEVEL 4
    #endif
#endif
