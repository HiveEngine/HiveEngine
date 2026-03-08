/**
 * Allocation Registry (Per-Allocator Tracking)
 *
 * Hash table that tracks all active allocations for a single allocator.
 * Used for leak detection, double-free detection, and statistics.
 *
 * Design:
 * - Per-allocator: Each allocator has its own registry
 * - Thread-safe: Protected by mutex (acceptable overhead in debug builds)
 * - Hash table: std::unordered_map (OK for debug code)
 * - Zero overhead: Only compiled when COMB_MEM_DEBUG=1
 *
 * Features:
 * - RegisterAllocation: Add allocation to registry
 * - UnregisterAllocation: Remove allocation (detects double-free)
 * - FindAllocation: Look up allocation info
 * - ReportLeaks: Print all unfreed allocations
 * - GetStats: Get allocation statistics
 *
 * Example:
 * @code
 *   #if COMB_MEM_DEBUG
 *       comb::debug::AllocationRegistry registry;
 *
 *       // Register allocation
 *       comb::debug::AllocationInfo info{};
 *       info.address = ptr;
 *       info.size = 64;
 *       registry.RegisterAllocation(info);
 *
 *       // Unregister (detects double-free)
 *       registry.UnregisterAllocation(ptr);
 *
 *       // Report leaks at shutdown
 *       registry.ReportLeaks("LinearAllocator");
 *   #endif
 * @endcode
 */

#pragma once

#include <comb/debug/mem_debug_config.h>

#if COMB_MEM_DEBUG

#include <hive/core/log.h>

#include <comb/combmodule.h>
#include <comb/debug/allocation_info.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace comb::debug
{

    /**
     * Allocation registry for a single allocator
     *
     * Thread-safe hash table that tracks all active allocations.
     * Provides leak detection, double-free detection, and statistics.
     *
     * Memory overhead: ~48 bytes per allocation (AllocationInfo + hash table node)
     * Performance: ~50-100ns per RegisterAllocation/UnregisterAllocation (mutex + hash insert/remove)
     */
    class AllocationRegistry
    {
    public:
        AllocationRegistry() = default;
        ~AllocationRegistry() = default;

        // Non-copyable, non-movable (contains std::atomic)
        AllocationRegistry(const AllocationRegistry&) = delete;
        AllocationRegistry& operator=(const AllocationRegistry&) = delete;
        AllocationRegistry(AllocationRegistry&&) noexcept = delete;
        AllocationRegistry& operator=(AllocationRegistry&&) noexcept = delete;

        // ========================================================================
        // Registration API
        // ========================================================================

        /**
         * Register a new allocation
         *
         * Adds allocation to the registry and updates statistics.
         * Asserts if address already exists (double allocation).
         *
         * @param info Allocation metadata
         *
         * Thread-safe: Yes (mutex protected)
         */
        void RegisterAllocation(const AllocationInfo& info)
        {
            hive::Assert(info.IsValid(), "Invalid AllocationInfo");

            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_allocations.find(info.m_address) != m_allocations.end())
            {
                hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] Double allocation detected! Address: {}, Size: {}",
                               info.m_address, info.m_size);
                hive::Assert(false, "Double allocation detected (same address allocated twice)");
                return;
            }

            m_allocations[info.m_address] = info;

            m_stats.m_totalAllocations++;
            m_stats.m_currentAllocations++;
            m_stats.m_currentBytesUsed += info.m_size;
            m_stats.m_totalBytesAllocated += info.m_size;
            m_stats.m_overheadBytes += info.GetTotalSize() - info.m_size;

            if (m_stats.m_currentBytesUsed > m_stats.m_peakBytesUsed)
            {
                m_stats.m_peakBytesUsed = m_stats.m_currentBytesUsed;
            }
        }

        /**
         * Unregister an allocation (deallocation)
         *
         * Removes allocation from registry and updates statistics.
         * Asserts if address not found (double-free or never allocated).
         *
         * @param address User pointer to deallocate
         * @return AllocationInfo if found, nullptr otherwise
         *
         * Thread-safe: Yes (mutex protected)
         */
        AllocationInfo* UnregisterAllocation(void* address)
        {
            hive::Assert(address != nullptr, "Cannot unregister nullptr");

            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_allocations.find(address);
            if (it == m_allocations.end())
            {
                hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] Double-free or invalid pointer detected! Address: {}",
                               address);
                hive::Assert(false, "Double-free or invalid pointer (not found in registry)");
                return nullptr;
            }

            m_stats.m_totalDeallocations++;
            m_stats.m_currentAllocations--;
            m_stats.m_currentBytesUsed -= it->second.m_size;
            m_stats.m_overheadBytes -= (it->second.GetTotalSize() - it->second.m_size);

            m_allocations.erase(it);

            return nullptr; // Allocation no longer valid
        }

        /**
         * Find allocation info by address
         *
         * @param address User pointer
         * @return Pointer to AllocationInfo if found, nullptr otherwise
         *
         * Thread-safe: Yes (mutex protected)
         *
         * NOTE: Returned pointer is only valid while mutex is held.
         * Do not store the pointer, copy the data instead.
         */
        AllocationInfo* FindAllocation(void* address)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_allocations.find(address);
            if (it != m_allocations.end())
            {
                return &it->second;
            }

            return nullptr;
        }

        /**
         * Find allocation info by address (const version)
         */
        const AllocationInfo* FindAllocation(void* address) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_allocations.find(address);
            if (it != m_allocations.end())
            {
                return &it->second;
            }

            return nullptr;
        }

        // ========================================================================
        // Statistics API
        // ========================================================================

        /**
         * Get current allocation statistics
         *
         * Thread-safe: Yes (mutex protected)
         */
        [[nodiscard]] AllocationStats GetStats() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_stats;
        }

        /**
         * Get number of active allocations
         */
        [[nodiscard]] size_t GetAllocationCount() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_allocations.size();
        }

        /**
         * Check if registry is empty (no leaks)
         */
        [[nodiscard]] bool IsEmpty() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_allocations.empty();
        }

        /**
         * Get next unique allocation ID (monotonically increasing)
         */
        [[nodiscard]] uint32_t GetNextAllocationId()
        {
            return m_nextAllocationId.fetch_add(1, std::memory_order_relaxed);
        }

        // ========================================================================
        // Leak Detection & Reporting
        // ========================================================================

        /**
         * Report memory leaks to log
         *
         * Prints all unfreed allocations with details.
         * Call this in allocator destructor.
         *
         * @param allocatorName Name of allocator (for logging)
         */
        void ReportLeaks(const char* allocatorName) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_allocations.empty())
            {
                hive::LogInfo(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] No memory leaks detected ✓", allocatorName);
                return;
            }

            hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] MEMORY LEAKS DETECTED: {} allocations not freed",
                           allocatorName, m_allocations.size());

            size_t totalLeaked = 0;
            for (const auto& [addr, info] : m_allocations)
            {
                totalLeaked += info.m_size;

                hive::LogError(comb::LOG_COMB_ROOT, "  LEAK #{}: Address={}, Size={} bytes, Tag={}, Thread={}",
                               info.m_allocationId, addr, info.m_size, info.GetTagOrDefault(), info.m_threadId);

#if COMB_MEM_DEBUG_CALLSTACKS
                if (info.callstackDepth > 0)
                {
                    hive::LogError(comb::LogCombRoot, "    Callstack:");
                    PrintCallstack(info.callstack, info.callstackDepth);
                }
#endif
            }

            hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Total leaked: {} bytes in {} allocations",
                           allocatorName, totalLeaked, m_allocations.size());
        }

        /**
         * Print allocation statistics to log
         *
         * @param allocatorName Name of allocator (for logging)
         */
        void PrintStats(const char* allocatorName) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            hive::LogInfo(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Statistics:", allocatorName);
            hive::LogInfo(comb::LOG_COMB_ROOT, "  Total allocations:   {}", m_stats.m_totalAllocations);
            hive::LogInfo(comb::LOG_COMB_ROOT, "  Total deallocations: {}", m_stats.m_totalDeallocations);
            hive::LogInfo(comb::LOG_COMB_ROOT, "  Active allocations:  {}", m_stats.m_currentAllocations);
            hive::LogInfo(comb::LOG_COMB_ROOT, "  Current memory used: {} bytes", m_stats.m_currentBytesUsed);
            hive::LogInfo(comb::LOG_COMB_ROOT, "  Peak memory used:    {} bytes", m_stats.m_peakBytesUsed);
            hive::LogInfo(comb::LOG_COMB_ROOT, "  Debug overhead:      {} bytes ({:.1f}%)", m_stats.m_overheadBytes,
                          m_stats.GetOverheadPercentage());
            hive::LogInfo(comb::LOG_COMB_ROOT, "  Fragmentation ratio: {:.2f}", m_stats.GetFragmentationRatio());
        }

        /**
         * Clear all allocations (DANGEROUS! Only for testing)
         *
         * This does NOT free memory, it just clears the tracking.
         * Use only for tests or when you know all memory is freed externally.
         */
        void Clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_allocations.clear();
            m_stats = AllocationStats{};
            m_nextAllocationId.store(1, std::memory_order_relaxed);
        }

        /**
         * Clear allocations from a specific address onwards
         *
         * Removes all allocations with address >= startAddress.
         * Used by marker-based allocators (Linear, Stack) when resetting to a marker.
         *
         * @param startAddress Clear all allocations >= this address
         */
        void ClearAllocationsFrom(void* startAddress)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            const uintptr_t start = reinterpret_cast<uintptr_t>(startAddress);

            for (auto it = m_allocations.begin(); it != m_allocations.end();)
            {
                const uintptr_t addr = reinterpret_cast<uintptr_t>(it->first);

                if (addr >= start)
                {
                    m_stats.m_currentAllocations--;
                    m_stats.m_currentBytesUsed -= it->second.m_size;
                    m_stats.m_overheadBytes -= (it->second.GetTotalSize() - it->second.m_size);

                    it = m_allocations.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        /**
         * Calculate total user bytes for allocations before a specific address
         *
         * Used by marker-based allocators to calculate GetUsedMemory() when resetting to a marker.
         *
         * @param endAddress Sum allocations with address < endAddress
         * @return Total user-requested bytes (excluding debug overhead)
         */
        [[nodiscard]] size_t CalculateBytesUsedUpTo(void* endAddress) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            const uintptr_t end = reinterpret_cast<uintptr_t>(endAddress);
            size_t total = 0;

            for (const auto& [addr, info] : m_allocations)
            {
                if (reinterpret_cast<uintptr_t>(addr) < end)
                {
                    total += info.m_size;
                }
            }

            return total;
        }

        /**
         * Count number of allocations before a specific address
         *
         * Used by marker-based allocators to track guard byte overhead when resetting to a marker.
         *
         * @param endAddress Count allocations with address < endAddress
         * @return Number of allocations before the address
         */
        [[nodiscard]] size_t CountAllocationsUpTo(void* endAddress) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            const uintptr_t end = reinterpret_cast<uintptr_t>(endAddress);
            size_t count = 0;

            for (const auto& [addr, info] : m_allocations)
            {
                if (reinterpret_cast<uintptr_t>(addr) < end)
                {
                    ++count;
                }
            }

            return count;
        }

    private:
        std::unordered_map<void*, AllocationInfo> m_allocations;
        AllocationStats m_stats{};
        std::atomic<uint32_t> m_nextAllocationId{1};
        mutable std::mutex m_mutex;
    };

} // namespace comb::debug

#endif // COMB_MEM_DEBUG
