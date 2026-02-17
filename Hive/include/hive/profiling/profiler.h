#pragma once

#include <hive/HiveConfig.h>

#if HIVE_FEATURE_PROFILER

    #include <tracy/Tracy.hpp>

    // Scoped zones
    #define HIVE_PROFILE_SCOPE              ZoneScoped
    #define HIVE_PROFILE_SCOPE_N(name)      ZoneScopedN(name)
    #define HIVE_PROFILE_SCOPE_C(color)     ZoneScopedC(color)

    // Frame markers
    #define HIVE_PROFILE_FRAME              FrameMark
    #define HIVE_PROFILE_FRAME_N(name)      FrameMarkNamed(name)

    // Memory tracking (named pools)
    #define HIVE_PROFILE_ALLOC(ptr, size, pool)     TracyAllocN(ptr, size, pool)
    #define HIVE_PROFILE_FREE(ptr, pool)             TracyFreeN(ptr, pool)

    // Thread naming
    #define HIVE_PROFILE_THREAD(name)       tracy::SetThreadName(name)

    // Lock tracking
    #define HIVE_PROFILE_LOCKABLE(type, var)                TracyLockable(type, var)
    #define HIVE_PROFILE_LOCKABLE_N(type, var, name)        TracyLockableN(type, var, name)
    #define HIVE_PROFILE_LOCKABLE_BASE(type)                LockableBase(type)

    // Value plots
    #define HIVE_PROFILE_PLOT(name, val)     TracyPlot(name, val)

    // Dynamic zone name (for runtime strings like system names)
    #define HIVE_PROFILE_ZONE_NAME(name, len)   ZoneName(name, len)

#else

    #define HIVE_PROFILE_SCOPE              (void)0
    #define HIVE_PROFILE_SCOPE_N(name)      (void)0
    #define HIVE_PROFILE_SCOPE_C(color)     (void)0

    #define HIVE_PROFILE_FRAME              (void)0
    #define HIVE_PROFILE_FRAME_N(name)      (void)0

    #define HIVE_PROFILE_ALLOC(ptr, size, pool)     (void)0
    #define HIVE_PROFILE_FREE(ptr, pool)             (void)0

    #define HIVE_PROFILE_THREAD(name)       (void)0

    #define HIVE_PROFILE_LOCKABLE(type, var)                type var
    #define HIVE_PROFILE_LOCKABLE_N(type, var, name)        type var
    #define HIVE_PROFILE_LOCKABLE_BASE(type)                type

    #define HIVE_PROFILE_PLOT(name, val)     (void)0

    #define HIVE_PROFILE_ZONE_NAME(name, len)   (void)0

#endif
