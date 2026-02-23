#pragma once

#if defined(_WIN32)
    #define HIVE_GAMEPLAY_EXPORT extern "C" __declspec(dllexport)
#else
    #define HIVE_GAMEPLAY_EXPORT extern "C" __attribute__((visibility("default")))
#endif
