#pragma once

#include <hive/HiveConfig.h>

#include <cstdint>

#define HIVE_GAMEPLAY_API_VERSION 2u

#define HIVE_GAMEPLAY_STRINGIZE_IMPL(x) #x
#define HIVE_GAMEPLAY_STRINGIZE(x) HIVE_GAMEPLAY_STRINGIZE_IMPL(x)

#define HIVE_GAMEPLAY_BUILD_SIGNATURE                                                                  \
    "api=" HIVE_GAMEPLAY_STRINGIZE(HIVE_GAMEPLAY_API_VERSION) ";debug=" HIVE_GAMEPLAY_STRINGIZE(       \
        HIVE_CONFIG_DEBUG) ";release=" HIVE_GAMEPLAY_STRINGIZE(HIVE_CONFIG_RELEASE) ";profile="        \
        HIVE_GAMEPLAY_STRINGIZE(HIVE_CONFIG_PROFILE) ";retail=" HIVE_GAMEPLAY_STRINGIZE(               \
            HIVE_CONFIG_RETAIL) ";editor=" HIVE_GAMEPLAY_STRINGIZE(HIVE_MODE_EDITOR) ";game="          \
        HIVE_GAMEPLAY_STRINGIZE(HIVE_MODE_GAME) ";headless=" HIVE_GAMEPLAY_STRINGIZE(                  \
            HIVE_MODE_HEADLESS) ";imgui=" HIVE_GAMEPLAY_STRINGIZE(HIVE_FEATURE_IMGUI) ";vulkan="       \
        HIVE_GAMEPLAY_STRINGIZE(HIVE_FEATURE_VULKAN) ";d3d12=" HIVE_GAMEPLAY_STRINGIZE(                \
            HIVE_FEATURE_D3D12) ";glfw=" HIVE_GAMEPLAY_STRINGIZE(HIVE_FEATURE_GLFW) ";mem_debug="      \
        HIVE_GAMEPLAY_STRINGIZE(HIVE_FEATURE_MEM_DEBUG) ";profiler=" HIVE_GAMEPLAY_STRINGIZE(          \
            HIVE_FEATURE_PROFILER) ";logging=" HIVE_GAMEPLAY_STRINGIZE(HIVE_FEATURE_LOGGING)           \
        ";asserts=" HIVE_GAMEPLAY_STRINGIZE(HIVE_FEATURE_ASSERTS) ";hot_reload="                       \
        HIVE_GAMEPLAY_STRINGIZE(HIVE_FEATURE_HOT_RELOAD) ";console="                                   \
        HIVE_GAMEPLAY_STRINGIZE(HIVE_FEATURE_CONSOLE)

#if defined(_WIN32)
#define HIVE_GAMEPLAY_EXPORT extern "C" __declspec(dllexport)
#else
#define HIVE_GAMEPLAY_EXPORT extern "C" __attribute__((visibility("default")))
#endif
