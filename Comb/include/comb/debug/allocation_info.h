/**
 * Allocation Metadata
 *
 * Stores information about each allocation for debugging purposes.
 * Only compiled when COMB_MEM_DEBUG=1 (zero overhead otherwise).
 *
 * Memory layout with guards:
 * ┌──────────────┬─────────────────────┬──────────────┐
 * │ GUARD_FRONT  │   User Data (size)  │  GUARD_BACK  │
 * │  (4 bytes)   │                     │   (4 bytes)  │
 * │ 0xDEADBEEF   │                     │  0xDEADBEEF  │
 * └──────────────┴─────────────────────┴──────────────┘
 *
 * Example:
 * @code
 *   #if COMB_MEM_DEBUG
 *       comb::debug::AllocationInfo info{};
 *       info.address = ptr;
 *       info.size = 64;
 *       info.tag = "Player Entity";
 *       registry.RegisterAllocation(info);
 *   #endif
 * @endcode
 */

#pragma once

#include <comb/debug/mem_debug_config.h>

#if COMB_MEM_DEBUG

#include <cstddef>
#include <cstdint>

namespace comb::debug
{

/**
 * Stores information about a single allocation for debugging.
 * Size: ~48 bytes without callstacks, ~176 bytes with callstacks
 */
struct AllocationInfo
{
    // ========================================================================
    // Core Information (Always Present)
    // ========================================================================

    void* address{nullptr};
    size_t size{0};
    size_t alignment{0};

    /**
     * Allocation timestamp (nanoseconds, monotonic)
     * Cross-platform: Use comb::debug::GetTimestamp()
     * Useful for tracking allocation order and lifetime
     */
    uint64_t timestamp{0};

    /**
     * Optional allocation tag (user-provided string literal)
     * Example: "Player Entity", "Temp Parse Buffer"
     * Can be nullptr if no tag provided
     *
     * NOTE: Assumed to be string literal (no ownership)
     */
    const char* tag{nullptr};

    uint32_t allocationId{0};
    uint32_t threadId{0};

    // ========================================================================
    // Optional: Callstack (Platform-Specific)
    // ========================================================================

#if COMB_MEM_DEBUG_CALLSTACKS
    /**
     * Callstack frames captured at allocation site
     * Platform-specific pointers (return addresses)
     * Size: 16 pointers * 8 bytes = 128 bytes
     */
    void* callstack[MaxCallstackDepth]{};

    /**
     * Actual number of frames captured (0-16)
     */
    uint32_t callstackDepth{0};
#endif

    // ========================================================================
    // Methods
    // ========================================================================

    /**
     * Check if this allocation info is valid
     */
    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return address != nullptr && size > 0;
    }

    /**
     * Get raw pointer (before guard bytes)
     * This is the actual pointer allocated from the underlying allocator
     */
    [[nodiscard]] void* GetRawPointer() const noexcept
    {
        return static_cast<std::byte*>(address) - GuardSize;
    }

    /**
     * Get total allocated size (including guard bytes)
     */
    [[nodiscard]] constexpr size_t GetTotalSize() const noexcept
    {
        return size + TotalGuardSize;
    }

    /**
     * Read front guard value (memcpy-safe for potentially misaligned addresses)
     */
    [[nodiscard]] uint32_t ReadGuardFront() const noexcept
    {
        return ReadGuard(static_cast<std::byte*>(address) - GuardSize);
    }

    /**
     * Read back guard value (memcpy-safe for potentially misaligned addresses)
     */
    [[nodiscard]] uint32_t ReadGuardBack() const noexcept
    {
        return ReadGuard(static_cast<std::byte*>(address) + size);
    }

    /**
     * Check if guard bytes are intact
     * Returns true if both guards match GuardMagic (0xDEADBEEF)
     */
    [[nodiscard]] bool CheckGuards() const noexcept
    {
        return ReadGuardFront() == GuardMagic &&
               ReadGuardBack() == GuardMagic;
    }

    /**
     * Get tag or default string
     */
    [[nodiscard]] const char* GetTagOrDefault(const char* defaultTag = "<no tag>") const noexcept
    {
        return tag ? tag : defaultTag;
    }
};

/**
 * Allocation statistics
 *
 * Aggregated statistics for an allocator or global tracker
 */
struct AllocationStats
{
    size_t totalAllocations{0};       // Total number of Allocate() calls
    size_t totalDeallocations{0};     // Total number of Deallocate() calls
    size_t currentAllocations{0};     // Active allocations (allocs - deallocs)

    size_t currentBytesUsed{0};       // Current memory used (user bytes)
    size_t peakBytesUsed{0};          // Peak memory used (high water mark)
    size_t totalBytesAllocated{0};    // Lifetime total bytes allocated

    size_t overheadBytes{0};          // Debug overhead (guards, metadata)

    /**
     * Calculate memory leak count
     */
    [[nodiscard]] constexpr size_t GetLeakCount() const noexcept
    {
        return totalAllocations - totalDeallocations;
    }

    /**
     * Calculate overhead percentage
     */
    [[nodiscard]] constexpr float GetOverheadPercentage() const noexcept
    {
        if (currentBytesUsed == 0) return 0.0f;
        return (static_cast<float>(overheadBytes) /
                static_cast<float>(currentBytesUsed + overheadBytes)) * 100.0f;
    }

    /**
     * Calculate fragmentation ratio (0.0 = no fragmentation, 1.0 = high)
     * This is a simplified metric based on allocation/deallocation patterns
     */
    [[nodiscard]] constexpr float GetFragmentationRatio() const noexcept
    {
        if (totalAllocations == 0) return 0.0f;

        // Simple heuristic: more deallocations relative to allocations = more fragmentation
        float deallocRatio = static_cast<float>(totalDeallocations) /
                             static_cast<float>(totalAllocations);

        // Clamp to [0, 1]
        return deallocRatio > 1.0f ? 1.0f : deallocRatio;
    }
};

} // namespace comb::debug

#endif // COMB_MEM_DEBUG
