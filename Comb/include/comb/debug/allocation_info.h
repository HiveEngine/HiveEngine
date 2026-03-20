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
        // Core Information (Always Present)

        void* m_address{nullptr};
        size_t m_size{0};
        size_t m_alignment{0};

        /**
         * Allocation timestamp (nanoseconds, monotonic)
         * Cross-platform: Use comb::debug::GetTimestamp()
         * Useful for tracking allocation order and lifetime
         */
        uint64_t m_timestamp{0};

        /**
         * Optional allocation tag (user-provided string literal)
         * Example: "Player Entity", "Temp Parse Buffer"
         * Can be nullptr if no tag provided
         *
         * NOTE: Assumed to be string literal (no ownership)
         */
        const char* m_tag{nullptr};

        uint32_t m_allocationId{0};
        uint32_t m_threadId{0};

        // Optional: Callstack (Platform-Specific)

#if COMB_MEM_DEBUG_CALLSTACKS
        /**
         * Callstack frames captured at allocation site
         * Platform-specific pointers (return addresses)
         * Size: 16 pointers * 8 bytes = 128 bytes
         */
        void* callstack[maxCallstackDepth]{};

        /**
         * Actual number of frames captured (0-16)
         */
        uint32_t callstackDepth{0};
#endif

        // Methods

        /**
         * Check if this allocation info is valid
         */
        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_address != nullptr && m_size > 0;
        }

        /**
         * Get raw pointer (before guard bytes)
         * This is the actual pointer allocated from the underlying allocator
         */
        [[nodiscard]] void* GetRawPointer() const noexcept
        {
            return static_cast<std::byte*>(m_address) - guardSize;
        }

        /**
         * Get total allocated size (including guard bytes)
         */
        [[nodiscard]] constexpr size_t GetTotalSize() const noexcept
        {
            return m_size + totalGuardSize;
        }

        /**
         * Read front guard value (memcpy-safe for potentially misaligned addresses)
         */
        [[nodiscard]] uint32_t ReadGuardFront() const noexcept
        {
            return ReadGuard(static_cast<std::byte*>(m_address) - guardSize);
        }

        /**
         * Read back guard value (memcpy-safe for potentially misaligned addresses)
         */
        [[nodiscard]] uint32_t ReadGuardBack() const noexcept
        {
            return ReadGuard(static_cast<std::byte*>(m_address) + m_size);
        }

        /**
         * Check if guard bytes are intact
         * Returns true if both guards match GuardMagic (0xDEADBEEF)
         */
        [[nodiscard]] bool CheckGuards() const noexcept
        {
            return ReadGuardFront() == guardMagic && ReadGuardBack() == guardMagic;
        }

        /**
         * Get tag or default string
         */
        [[nodiscard]] const char* GetTagOrDefault(const char* defaultTag = "<no tag>") const noexcept
        {
            return m_tag ? m_tag : defaultTag;
        }
    };

    /**
     * Allocation statistics
     *
     * Aggregated statistics for an allocator or global tracker
     */
    struct AllocationStats
    {
        size_t m_totalAllocations{0};   // Total number of Allocate() calls
        size_t m_totalDeallocations{0}; // Total number of Deallocate() calls
        size_t m_currentAllocations{0}; // Active allocations (allocs - deallocs)

        size_t m_currentBytesUsed{0};    // Current memory used (user bytes)
        size_t m_peakBytesUsed{0};       // Peak memory used (high water mark)
        size_t m_totalBytesAllocated{0}; // Lifetime total bytes allocated

        size_t m_overheadBytes{0}; // Debug overhead (guards, metadata)

        /**
         * Calculate memory leak count
         */
        [[nodiscard]] constexpr size_t GetLeakCount() const noexcept
        {
            return m_totalAllocations - m_totalDeallocations;
        }

        /**
         * Calculate overhead percentage
         */
        [[nodiscard]] constexpr float GetOverheadPercentage() const noexcept
        {
            if (m_currentBytesUsed == 0)
                return 0.0f;
            return (static_cast<float>(m_overheadBytes) / static_cast<float>(m_currentBytesUsed + m_overheadBytes)) *
                   100.0f;
        }

        /**
         * Calculate fragmentation ratio (0.0 = no fragmentation, 1.0 = high)
         * This is a simplified metric based on allocation/deallocation patterns
         */
        [[nodiscard]] constexpr float GetFragmentationRatio() const noexcept
        {
            if (m_totalAllocations == 0)
                return 0.0f;

            // Simple heuristic: more deallocations relative to allocations = more fragmentation
            float deallocRatio = static_cast<float>(m_totalDeallocations) / static_cast<float>(m_totalAllocations);

            // Clamp to [0, 1]
            return deallocRatio > 1.0f ? 1.0f : deallocRatio;
        }
    };

} // namespace comb::debug

#endif // COMB_MEM_DEBUG
