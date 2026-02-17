/**
 * Platform-Specific Utilities for Memory Debugging
 *
 * Cross-platform abstractions for:
 * - High-resolution timestamps
 * - Thread ID retrieval
 * - Callstack capture (optional)
 *
 * Supported platforms:
 * - Windows (MSVC, Clang-CL)
 * - Linux (GCC, Clang)
 * - macOS (Clang, GCC)
 *
 * Design: Zero overhead when COMB_MEM_DEBUG=0
 */

#pragma once

#include <comb/debug/mem_debug_config.h>

#if COMB_MEM_DEBUG

#include <hive/core/assert.h>
#include <cstdint>
#include <cstdio>

// Platform includes (use Hive's platform detection macros)
#if HIVE_PLATFORM_WINDOWS
    #include <windows.h>
    #include <intrin.h>  // For __rdtsc
#elif HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
    #include <pthread.h>
    #include <time.h>
    #include <sys/time.h>
    #if defined(__x86_64__) || defined(__i386__)
        #include <x86intrin.h>  // For __rdtsc on GCC/Clang
    #endif
#endif

// Callstack capture (optional)
#if COMB_MEM_DEBUG_CALLSTACKS
    #if HIVE_PLATFORM_WINDOWS
        #include <dbghelp.h>
    #elif HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
        #include <execinfo.h>
    #endif
#endif

namespace comb::debug
{

// ============================================================================
// High-Resolution Timestamp
// ============================================================================

/**
 * Get high-resolution timestamp (monotonic, nanoseconds)
 *
 * Implementations:
 * - Windows: QueryPerformanceCounter (QPC)
 * - Linux: clock_gettime(CLOCK_MONOTONIC)
 * - macOS: clock_gettime(CLOCK_MONOTONIC) or mach_absolute_time()
 *
 * Returns: Monotonic timestamp in nanoseconds (uint64_t)
 *
 * NOTE: Does NOT use __rdtsc() because:
 * - Not portable to ARM (Nintendo Switch, mobile)
 * - Can be affected by CPU frequency scaling
 * - Requires serialization for accuracy
 *
 */
inline uint64_t GetTimestamp() noexcept
{
#if HIVE_PLATFORM_WINDOWS
    // Windows: Use QueryPerformanceCounter (QPC)
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);

    // Convert to nanoseconds: (counter * 1e9) / frequency
    return static_cast<uint64_t>(
        (static_cast<uint64_t>(counter.QuadPart) * 1000000000ULL) /
        static_cast<uint64_t>(frequency.QuadPart)
    );

#elif HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
    // POSIX: Use clock_gettime (CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    // Convert to nanoseconds
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<uint64_t>(ts.tv_nsec);

#else
    #error "Unsupported platform for GetTimestamp()"
#endif
}

/**
 * Check if CPU cycle counter is available (compile-time)
 * True on x86/x64, false on ARM/RISC-V
 */
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
inline constexpr bool kHasCycleCounter = true;
#else
inline constexpr bool kHasCycleCounter = false;
#endif

/**
 * Get CPU cycle counter (OPTIONAL, x86/x64 only)
 *
 * This is faster than GetTimestamp() but:
 * - Only works on x86/x64 (Intel/AMD)
 * - Not portable to ARM
 * - Affected by frequency scaling
 *
 * Use GetTimestamp() for portable code.
 * Use GetCycleCounter() only for x86-specific profiling.
 *
 * Check availability with: if constexpr (kHasCycleCounter)
 */
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
inline uint64_t GetCycleCounter() noexcept
{
    return __rdtsc();
}
#endif

// ============================================================================
// Thread ID
// ============================================================================

/**
 * Get current thread ID (cross-platform)
 *
 * Returns: Platform-specific thread ID as uint32_t
 * - Windows: GetCurrentThreadId()
 * - Linux/macOS: pthread_self() (cast to uint32_t)
 *
 * NOTE: This is NOT the same as std::thread::id (which is opaque).
 * We need a numeric ID for display/logging purposes.
 *
 */
inline uint32_t GetThreadId() noexcept
{
#if HIVE_PLATFORM_WINDOWS
    return static_cast<uint32_t>(GetCurrentThreadId());

#elif HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
    // pthread_self() returns pthread_t (opaque type, often pointer-sized)
    // Cast to uintptr_t first, then truncate to uint32_t
    pthread_t tid = pthread_self();
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tid));

#else
    #error "Unsupported platform for GetThreadId()"
#endif
}

// ============================================================================
// Callstack Capture (Optional, Expensive)
// ============================================================================

#if COMB_MEM_DEBUG_CALLSTACKS

/**
 * Capture callstack (platform-specific)
 *
 * Captures up to MaxCallstackDepth (16) frames.
 * Skips the first frame (this function itself).
 *
 * @param frames Output array (must have space for MaxCallstackDepth)
 * @param depth Output: actual number of frames captured
 *
 * Performance: VERY SLOW
 *
 * Only use when debugging specific leaks!
 */
inline void CaptureCallstack(void** frames, uint32_t& depth) noexcept
{
    hive::Assert(frames != nullptr, "frames must not be null");

#if HIVE_PLATFORM_WINDOWS
    // Windows: CaptureStackBackTrace (fast, kernel32.dll)
    // Skip 1 frame (this function)
    depth = static_cast<uint32_t>(
        CaptureStackBackTrace(1, MaxCallstackDepth, frames, nullptr)
    );

#elif HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
    // POSIX: backtrace (requires linking with -rdynamic for symbols)
    int count = backtrace(frames, MaxCallstackDepth);
    depth = count > 0 ? static_cast<uint32_t>(count) : 0;

    // Skip first frame (this function)
    if (depth > 0)
    {
        for (uint32_t i = 0; i < depth - 1; ++i)
        {
            frames[i] = frames[i + 1];
        }
        --depth;
    }

#else
    #error "Unsupported platform for CaptureCallstack()"
#endif
}

/**
 * Print callstack to log (platform-specific symbolication)
 *
 * Resolves addresses to function names and prints to hive::Log.
 *
 * @param frames Callstack frames from CaptureCallstack()
 * @param depth Number of frames
 *
 * Requirements:
 * - Windows: dbghelp.lib linked, symbols available (.pdb)
 * - Linux/macOS: Compile with -rdynamic, or use addr2line
 */
void PrintCallstack(void* const* frames, uint32_t depth);

#endif // COMB_MEM_DEBUG_CALLSTACKS

// ============================================================================
// Utility: Format Time Duration
// ============================================================================

/**
 * Format timestamp difference as human-readable string
 *
 * Example: FormatDuration(1500000000) -> "1.5s"
 *
 * @param nanos Duration in nanoseconds
 * @return Formatted string (e.g., "1.5ms", "500ns", "2.3s")
 */
inline const char* FormatDuration(uint64_t nanos)
{
    static thread_local char buffer[64];

    if (nanos < 1000)
    {
        snprintf(buffer, sizeof(buffer), "%lluns", nanos);
    }
    else if (nanos < 1000000)
    {
        snprintf(buffer, sizeof(buffer), "%.1fµs", static_cast<double>(nanos) / 1000.0);
    }
    else if (nanos < 1000000000)
    {
        snprintf(buffer, sizeof(buffer), "%.1fms", static_cast<double>(nanos) / 1000000.0);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%.1fs", static_cast<double>(nanos) / 1000000000.0);
    }

    return buffer;
}

} // namespace comb::debug

#endif // COMB_MEM_DEBUG
