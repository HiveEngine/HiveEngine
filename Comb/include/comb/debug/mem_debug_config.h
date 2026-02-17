/**
 * Memory Debugging Configuration
 *
 * Feature flags for Comb memory debugging system.
 * All features compile to nothing when COMB_MEM_DEBUG=0 (zero overhead).
 *
 * Build configuration:
 * - COMB_MEM_DEBUG=0: Zero overhead, all tracking disabled (default)
 * - COMB_MEM_DEBUG=1: Enable memory debugging (slow! 2-10x overhead)
 * - COMB_MEM_DEBUG_CALLSTACKS=1: Enable callstack capture (very slow! 10-100x)
 *
 * Usage:
 * @code
 *   // CMake
 *   option(COMB_ENABLE_MEM_DEBUG "Enable memory debugging" OFF)
 *   option(COMB_ENABLE_CALLSTACKS "Enable callstack capture" OFF)
 *
 *   // Code
 *   #if COMB_MEM_DEBUG_LEAK_DETECTION
 *       registry_.ReportLeaks(GetName());
 *   #endif
 * @endcode
 */

#pragma once

// ============================================================================
// Build Configuration (Set by CMake)
// ============================================================================

// Default to disabled if not set by CMake
#ifndef COMB_MEM_DEBUG
    #define COMB_MEM_DEBUG 0
#endif

#ifndef COMB_MEM_DEBUG_CALLSTACKS
    #define COMB_MEM_DEBUG_CALLSTACKS 0
#endif

// ============================================================================
// Feature Flags
// ============================================================================

#if COMB_MEM_DEBUG

    // ========================================================================
    // Core Features (Always Enabled)
    // ========================================================================

    /**
     * Leak Detection
     * - Tracks all allocations in hash table
     * - Reports unfreed allocations on allocator destruction
     * - Overhead: Low (hash table insert/remove)
     */
    #define COMB_MEM_DEBUG_LEAK_DETECTION 1

    /**
     * Double-Free Detection
     * - Checks if pointer exists in registry before deallocation
     * - Asserts if pointer not found (already freed or never allocated)
     * - Overhead: Low (hash table lookup)
     */
    #define COMB_MEM_DEBUG_DOUBLE_FREE 1

    /**
     * Buffer Overrun Detection
     * - Adds guard bytes (0xDEADBEEF) before and after allocation
     * - Checks guards on deallocation
     * - Overhead: Low (+8 bytes per allocation)
     */
    #define COMB_MEM_DEBUG_BUFFER_OVERRUN 1

    /**
     * Allocation Tracking
     * - Stores metadata for each allocation (size, alignment, timestamp, tag)
     * - Overhead: Low (~32 bytes per allocation in hash table)
     */
    #define COMB_MEM_DEBUG_TRACKING 1

    /**
     * Memory Profiling Statistics
     * - Tracks peak usage, allocation count, fragmentation
     * - Overhead: Negligible (few counters)
     */
    #define COMB_MEM_DEBUG_STATS 1

    // ========================================================================
    // Optional Features (Can be Disabled for Performance)
    // ========================================================================

    /**
     * Use-After-Free Detection
     * - Fills freed memory with pattern (0xFE)
     * - Helps detect reads from freed memory
     * - Overhead: Medium (memset on every deallocation)
     * - Can be disabled: #define COMB_MEM_DEBUG_USE_AFTER_FREE 0
     */
    #ifndef COMB_MEM_DEBUG_USE_AFTER_FREE
        #define COMB_MEM_DEBUG_USE_AFTER_FREE 1  // Enabled by default
    #endif

    /**
     * Allocation History Ring Buffer
     * - Keeps ring buffer of recent allocations
     * - Useful for post-mortem debugging
     * - Overhead: Medium (ring buffer storage)
     * - Size configurable via COMB_MEM_DEBUG_HISTORY_SIZE
     */
    #ifndef COMB_MEM_DEBUG_HISTORY
        #define COMB_MEM_DEBUG_HISTORY 1  // Enabled by default
    #endif

    #ifndef COMB_MEM_DEBUG_HISTORY_SIZE
        #define COMB_MEM_DEBUG_HISTORY_SIZE 1000  // Last 1000 allocations
    #endif

    // ========================================================================
    // Expensive Features (Disabled by Default)
    // ========================================================================

    /**
     * Callstack Capture
     * - Captures callstack at allocation site (16 frames)
     * - Platform-specific (Windows: CaptureStackBackTrace, POSIX: backtrace)
     * - Overhead: VERY HIGH (10-100x slower allocations!)
     * - Only enable when debugging specific leaks
     * - Set by CMake: -DCOMB_ENABLE_CALLSTACKS=ON
     */
    // COMB_MEM_DEBUG_CALLSTACKS already defined by CMake

    // ========================================================================
    // Debug Constants
    // ========================================================================

    #include <cstring>  // For memcpy

    namespace comb::debug
    {
        // Guard magic value (0xDEADBEEF)
        constexpr uint32_t GuardMagic = 0xDEADBEEF;

        // Memory patterns
        constexpr uint8_t AllocatedMemoryPattern = 0xAA;  // "Allocated" - 0b10101010
        constexpr uint8_t FreedMemoryPattern = 0xFE;      // "Freed"     - 0b11111110
        constexpr uint8_t GuardBytePattern = 0xBE;        // "BEef"      - 0b10111110

        // Guard size (before and after allocation)
        constexpr size_t GuardSize = sizeof(uint32_t);  // 4 bytes
        constexpr size_t TotalGuardSize = 2 * GuardSize;  // 8 bytes total

        // Callstack depth (if enabled)
        constexpr uint32_t MaxCallstackDepth = 16;

        // Safe guard read/write (handles potentially misaligned back guards)
        inline void WriteGuard(void* addr) noexcept
        {
            uint32_t magic = GuardMagic;
            std::memcpy(addr, &magic, sizeof(uint32_t));
        }

        inline uint32_t ReadGuard(const void* addr) noexcept
        {
            uint32_t value;
            std::memcpy(&value, addr, sizeof(uint32_t));
            return value;
        }
    }

#else // COMB_MEM_DEBUG = 0

    // ========================================================================
    // All Features Disabled (Zero Overhead)
    // ========================================================================

    #define COMB_MEM_DEBUG_LEAK_DETECTION 0
    #define COMB_MEM_DEBUG_DOUBLE_FREE 0
    #define COMB_MEM_DEBUG_BUFFER_OVERRUN 0
    #define COMB_MEM_DEBUG_TRACKING 0
    #define COMB_MEM_DEBUG_STATS 0
    #define COMB_MEM_DEBUG_USE_AFTER_FREE 0
    #define COMB_MEM_DEBUG_HISTORY 0
    #define COMB_MEM_DEBUG_HISTORY_SIZE 0

    // Callstack always disabled if MEM_DEBUG is off
    #undef COMB_MEM_DEBUG_CALLSTACKS
    #define COMB_MEM_DEBUG_CALLSTACKS 0

#endif // COMB_MEM_DEBUG

// ============================================================================
// Compile-Time Constants
// ============================================================================

namespace comb::debug
{
    /**
     * Compile-time constant: is memory debugging enabled?
     * Use with `if constexpr` for zero-overhead conditional compilation
     *
     * Example:
     * @code
     *   if constexpr (comb::debug::kMemDebugEnabled) {
     *       registry_.ReportLeaks(GetName());
     *   }
     * @endcode
     */
    inline constexpr bool kMemDebugEnabled = (COMB_MEM_DEBUG != 0);

    /**
     * Compile-time constant: is callstack capture enabled?
     */
    inline constexpr bool kCallstacksEnabled = (COMB_MEM_DEBUG_CALLSTACKS != 0);

    /**
     * Compile-time constant: is leak detection enabled?
     */
    inline constexpr bool kLeakDetectionEnabled = (COMB_MEM_DEBUG_LEAK_DETECTION != 0);

    /**
     * Compile-time constant: is use-after-free detection enabled?
     */
    inline constexpr bool kUseAfterFreeEnabled = (COMB_MEM_DEBUG_USE_AFTER_FREE != 0);
}

// ============================================================================
// Static Assertions (Compile-Time Checks)
// ============================================================================

// Ensure callstacks are only enabled when MEM_DEBUG is enabled
static_assert(
    COMB_MEM_DEBUG_CALLSTACKS == 0 || COMB_MEM_DEBUG == 1,
    "COMB_MEM_DEBUG_CALLSTACKS requires COMB_MEM_DEBUG=1"
);

// ============================================================================
// Summary Log (Disabled - too verbose during compilation)
// ============================================================================
//
// Memory debugging configuration:
// - COMB_MEM_DEBUG: Enabled/Disabled
// - COMB_MEM_DEBUG_CALLSTACKS: Enabled/Disabled
// - COMB_MEM_DEBUG_HISTORY: Enabled/Disabled
//
// See CMakeLists.txt for build configuration
