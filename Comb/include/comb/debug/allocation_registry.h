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

#include <comb/debug/allocation_info.h>
#include <hive/core/log.h>
#include <comb/combmodule.h>

#include <unordered_map>
#include <mutex>
#include <atomic>

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

        std::lock_guard<std::mutex> lock(mutex_);

        if (allocations_.find(info.address) != allocations_.end())
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] Double allocation detected! Address: {}, Size: {}",
                           info.address, info.size);
            hive::Assert(false, "Double allocation detected (same address allocated twice)");
            return;
        }

        allocations_[info.address] = info;

        stats_.totalAllocations++;
        stats_.currentAllocations++;
        stats_.currentBytesUsed += info.size;
        stats_.totalBytesAllocated += info.size;
        stats_.overheadBytes += info.GetTotalSize() - info.size;

        if (stats_.currentBytesUsed > stats_.peakBytesUsed)
        {
            stats_.peakBytesUsed = stats_.currentBytesUsed;
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

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = allocations_.find(address);
        if (it == allocations_.end())
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] Double-free or invalid pointer detected! Address: {}",
                           address);
            hive::Assert(false, "Double-free or invalid pointer (not found in registry)");
            return nullptr;
        }

        stats_.totalDeallocations++;
        stats_.currentAllocations--;
        stats_.currentBytesUsed -= it->second.size;
        stats_.overheadBytes -= (it->second.GetTotalSize() - it->second.size);

        allocations_.erase(it);

        return nullptr;  // Allocation no longer valid
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
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = allocations_.find(address);
        if (it != allocations_.end())
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
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = allocations_.find(address);
        if (it != allocations_.end())
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
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    /**
     * Get number of active allocations
     */
    [[nodiscard]] size_t GetAllocationCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocations_.size();
    }

    /**
     * Check if registry is empty (no leaks)
     */
    [[nodiscard]] bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocations_.empty();
    }

    /**
     * Get next unique allocation ID (monotonically increasing)
     */
    [[nodiscard]] uint32_t GetNextAllocationId()
    {
        return nextAllocationId_.fetch_add(1, std::memory_order_relaxed);
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
        std::lock_guard<std::mutex> lock(mutex_);

        if (allocations_.empty())
        {
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG] [{}] No memory leaks detected ✓",
                          allocatorName);
            return;
        }

        hive::LogError(comb::LogCombRoot,
                       "[MEM_DEBUG] [{}] MEMORY LEAKS DETECTED: {} allocations not freed",
                       allocatorName, allocations_.size());

        size_t totalLeaked = 0;
        for (const auto& [addr, info] : allocations_)
        {
            totalLeaked += info.size;

            hive::LogError(comb::LogCombRoot,
                           "  LEAK #{}: Address={}, Size={} bytes, Tag={}, Thread={}",
                           info.allocationId, addr, info.size,
                           info.GetTagOrDefault(), info.threadId);

#if COMB_MEM_DEBUG_CALLSTACKS
            if (info.callstackDepth > 0)
            {
                hive::LogError(comb::LogCombRoot, "    Callstack:");
                PrintCallstack(info.callstack, info.callstackDepth);
            }
#endif
        }

        hive::LogError(comb::LogCombRoot,
                       "[MEM_DEBUG] [{}] Total leaked: {} bytes in {} allocations",
                       allocatorName, totalLeaked, allocations_.size());
    }

    /**
     * Print allocation statistics to log
     *
     * @param allocatorName Name of allocator (for logging)
     */
    void PrintStats(const char* allocatorName) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] [{}] Statistics:", allocatorName);
        hive::LogInfo(comb::LogCombRoot,
                      "  Total allocations:   {}", stats_.totalAllocations);
        hive::LogInfo(comb::LogCombRoot,
                      "  Total deallocations: {}", stats_.totalDeallocations);
        hive::LogInfo(comb::LogCombRoot,
                      "  Active allocations:  {}", stats_.currentAllocations);
        hive::LogInfo(comb::LogCombRoot,
                      "  Current memory used: {} bytes", stats_.currentBytesUsed);
        hive::LogInfo(comb::LogCombRoot,
                      "  Peak memory used:    {} bytes", stats_.peakBytesUsed);
        hive::LogInfo(comb::LogCombRoot,
                      "  Debug overhead:      {} bytes ({:.1f}%)",
                      stats_.overheadBytes, stats_.GetOverheadPercentage());
        hive::LogInfo(comb::LogCombRoot,
                      "  Fragmentation ratio: {:.2f}", stats_.GetFragmentationRatio());
    }

    /**
     * Clear all allocations (DANGEROUS! Only for testing)
     *
     * This does NOT free memory, it just clears the tracking.
     * Use only for tests or when you know all memory is freed externally.
     */
    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_.clear();
        stats_ = AllocationStats{};
        nextAllocationId_.store(1, std::memory_order_relaxed);
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
        std::lock_guard<std::mutex> lock(mutex_);

        const uintptr_t start = reinterpret_cast<uintptr_t>(startAddress);

        for (auto it = allocations_.begin(); it != allocations_.end(); )
        {
            const uintptr_t addr = reinterpret_cast<uintptr_t>(it->first);

            if (addr >= start)
            {
                stats_.currentAllocations--;
                stats_.currentBytesUsed -= it->second.size;
                stats_.overheadBytes -= (it->second.GetTotalSize() - it->second.size);

                it = allocations_.erase(it);
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
        std::lock_guard<std::mutex> lock(mutex_);

        const uintptr_t end = reinterpret_cast<uintptr_t>(endAddress);
        size_t total = 0;

        for (const auto& [addr, info] : allocations_)
        {
            if (reinterpret_cast<uintptr_t>(addr) < end)
            {
                total += info.size;
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
        std::lock_guard<std::mutex> lock(mutex_);

        const uintptr_t end = reinterpret_cast<uintptr_t>(endAddress);
        size_t count = 0;

        for (const auto& [addr, info] : allocations_)
        {
            if (reinterpret_cast<uintptr_t>(addr) < end)
            {
                ++count;
            }
        }

        return count;
    }

private:
    std::unordered_map<void*, AllocationInfo> allocations_;
    AllocationStats stats_{};
    std::atomic<uint32_t> nextAllocationId_{1};
    mutable std::mutex mutex_;
};

} // namespace comb::debug

#endif // COMB_MEM_DEBUG
