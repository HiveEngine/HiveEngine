/**
 * Global Memory Tracker (Singleton)
 *
 * Engine-wide memory tracking across all allocators.
 * Provides bird's-eye view of memory usage for profiling and debugging.
 *
 * Architecture:
 * - Singleton: Single instance for entire engine
 * - Hybrid: Per-allocator registries + global aggregation
 * - Thread-safe: Protected by mutex
 * - Zero overhead: Only compiled when COMB_MEM_DEBUG=1
 *
 * Features:
 * - Register/unregister allocators
 * - Query all allocations across all allocators
 * - Engine-wide statistics (total memory, peak, breakdown by allocator)
 * - Export to JSON for visualization
 *
 * Example:
 * @code
 *   #if COMB_MEM_DEBUG
 *       // Allocators register themselves
 *       comb::debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(
 *           "LinearAllocator", &registry_
 *       );
 *
 *       // Query engine-wide stats
 *       auto stats = comb::debug::GlobalMemoryTracker::GetInstance().GetGlobalStats();
 *       hive::Log("Total memory: {} bytes", stats.currentBytesUsed);
 *
 *       // Print all allocators
 *       comb::debug::GlobalMemoryTracker::GetInstance().PrintAllAllocators();
 *   #endif
 * @endcode
 */

#pragma once

#include <comb/debug/mem_debug_config.h>

#if COMB_MEM_DEBUG

#include <comb/debug/allocation_info.h>
#include <comb/debug/allocation_registry.h>
#include <comb/debug/platform_utils.h>
#include <hive/core/log.h>
#include <comb/combmodule.h>

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace comb::debug
{

/**
 * Allocator info for global tracking
 */
struct AllocatorInfo
{
    const char* name;                    // Allocator name (e.g., "LinearAllocator")
    AllocationRegistry* registry;        // Pointer to allocator's registry
    uint64_t registrationTime;           // When allocator was registered (nanoseconds)
};

/**
 * Global memory tracker (singleton)
 *
 * Tracks all allocators in the engine for engine-wide profiling.
 * Thread-safe singleton using Meyer's idiom.
 */
class GlobalMemoryTracker
{
public:
    /**
     * Get singleton instance (Meyer's singleton, thread-safe since C++11)
     */
    static GlobalMemoryTracker& GetInstance()
    {
        static GlobalMemoryTracker instance;
        return instance;
    }

    // Non-copyable, non-movable (singleton)
    GlobalMemoryTracker(const GlobalMemoryTracker&) = delete;
    GlobalMemoryTracker& operator=(const GlobalMemoryTracker&) = delete;
    GlobalMemoryTracker(GlobalMemoryTracker&&) = delete;
    GlobalMemoryTracker& operator=(GlobalMemoryTracker&&) = delete;

    // ========================================================================
    // Allocator Registration
    // ========================================================================

    /**
     * Register an allocator with the global tracker
     *
     * Allocators should register themselves in their constructor.
     * The name should be a string literal (no ownership).
     *
     * @param name Allocator name (string literal)
     * @param registry Pointer to allocator's registry
     *
     * Thread-safe: Yes (mutex protected)
     */
    void RegisterAllocator(const char* name, AllocationRegistry* registry)
    {
        hive::Assert(name != nullptr, "Allocator name cannot be null");
        hive::Assert(registry != nullptr, "Registry cannot be null");

        std::lock_guard<std::mutex> lock(mutex_);

        // Create unique key: name + address (allows multiple allocators with same name)
        std::string key = std::string(name) + "_" + std::to_string(reinterpret_cast<uintptr_t>(registry));

        AllocatorInfo info{};
        info.name = name;
        info.registry = registry;
        info.registrationTime = comb::debug::GetTimestamp();

        allocators_[key] = info;

        hive::LogTrace(comb::LogCombRoot,
                       "[MEM_DEBUG] Registered allocator: {} ({})",
                       name, reinterpret_cast<void*>(registry));
    }

    /**
     * Unregister an allocator
     *
     * Allocators should unregister themselves in their destructor.
     *
     * @param registry Pointer to allocator's registry
     *
     * Thread-safe: Yes (mutex protected)
     */
    void UnregisterAllocator(AllocationRegistry* registry)
    {
        hive::Assert(registry != nullptr, "Registry cannot be null");

        std::lock_guard<std::mutex> lock(mutex_);

        // Find and remove allocator
        for (auto it = allocators_.begin(); it != allocators_.end(); ++it)
        {
            if (it->second.registry == registry)
            {
                hive::LogTrace(comb::LogCombRoot,
                               "[MEM_DEBUG] Unregistered allocator: {}",
                               it->second.name);
                allocators_.erase(it);
                return;
            }
        }

        hive::LogWarning(comb::LogCombRoot,
                         "[MEM_DEBUG] Attempted to unregister unknown allocator: {}",
                         reinterpret_cast<void*>(registry));
    }

    // ========================================================================
    // Global Statistics
    // ========================================================================

    /**
     * Get engine-wide aggregated statistics
     *
     * Sums statistics across all registered allocators.
     *
     * Thread-safe: Yes (mutex protected)
     */
    [[nodiscard]] AllocationStats GetGlobalStats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        AllocationStats globalStats{};

        for (const auto& [key, info] : allocators_)
        {
            AllocationStats allocStats = info.registry->GetStats();

            globalStats.totalAllocations += allocStats.totalAllocations;
            globalStats.totalDeallocations += allocStats.totalDeallocations;
            globalStats.currentAllocations += allocStats.currentAllocations;
            globalStats.currentBytesUsed += allocStats.currentBytesUsed;
            globalStats.totalBytesAllocated += allocStats.totalBytesAllocated;
            globalStats.overheadBytes += allocStats.overheadBytes;

            // Sum peaks across allocators (upper bound — individual peaks may not coincide)
            globalStats.peakBytesUsed += allocStats.peakBytesUsed;
        }

        return globalStats;
    }

    /**
     * Get number of registered allocators
     */
    [[nodiscard]] size_t GetAllocatorCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocators_.size();
    }

    // ========================================================================
    // Reporting & Visualization
    // ========================================================================

    /**
     * Print all registered allocators and their statistics
     */
    void PrintAllAllocators() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (allocators_.empty())
        {
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG] No allocators registered");
            return;
        }

        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] ========== Global Memory Report ==========");
        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] Registered allocators: {}", allocators_.size());

        size_t totalCurrent = 0;
        size_t totalPeak = 0;

        for (const auto& [key, info] : allocators_)
        {
            AllocationStats stats = info.registry->GetStats();

            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG] ---");
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG] Allocator: {}", info.name);
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG]   Current: {} bytes ({} allocations)",
                          stats.currentBytesUsed, stats.currentAllocations);
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG]   Peak: {} bytes", stats.peakBytesUsed);
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG]   Total allocs/deallocs: {} / {}",
                          stats.totalAllocations, stats.totalDeallocations);

            totalCurrent += stats.currentBytesUsed;
            totalPeak = (std::max)(totalPeak, stats.peakBytesUsed);
        }

        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] ---");
        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] TOTAL Current: {} bytes ({} MB)",
                      totalCurrent, totalCurrent / (1024 * 1024));
        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] TOTAL Peak: {} bytes ({} MB)",
                      totalPeak, totalPeak / (1024 * 1024));
        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] ============================================");
    }

    /**
     * Print engine-wide leak report
     *
     * Checks all allocators for leaks and prints summary.
     */
    void PrintLeakReport() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        size_t totalLeaks = 0;
        size_t totalLeakedBytes = 0;
        std::vector<const char*> leakyAllocators;

        for (const auto& [key, info] : allocators_)
        {
            size_t leakCount = info.registry->GetAllocationCount();
            if (leakCount > 0)
            {
                AllocationStats stats = info.registry->GetStats();
                totalLeaks += leakCount;
                totalLeakedBytes += stats.currentBytesUsed;
                leakyAllocators.push_back(info.name);
            }
        }

        if (totalLeaks == 0)
        {
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG] ========== Global Leak Report ==========");
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG] NO MEMORY LEAKS DETECTED ✓");
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG] =========================================");
        }
        else
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] ========== Global Leak Report ==========");
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] MEMORY LEAKS DETECTED!");
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] Total leaks: {} allocations", totalLeaks);
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] Total leaked: {} bytes ({} MB)",
                           totalLeakedBytes, totalLeakedBytes / (1024 * 1024));
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] Leaky allocators:");

            for (const char* name : leakyAllocators)
            {
                hive::LogError(comb::LogCombRoot,
                               "[MEM_DEBUG]   - {}", name);
            }

            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] =========================================");
        }
    }

    /**
     * Export all allocator stats to JSON string
     *
     * Generates JSON for external visualization tools.
     *
     * Format:
     * {
     *   "allocators": [
     *     {
     *       "name": "LinearAllocator",
     *       "currentBytes": 1024,
     *       "peakBytes": 2048,
     *       "allocations": 10,
     *       ...
     *     }
     *   ],
     *   "global": {
     *     "currentBytes": 5120,
     *     "peakBytes": 10240,
     *     ...
     *   }
     * }
     *
     * @return JSON string
     */
    [[nodiscard]] std::string ExportToJSON() const;

private:
    GlobalMemoryTracker() = default;
    ~GlobalMemoryTracker() = default;

    /**
     * Get global stats without locking (internal use only)
     * Assumes mutex is already held by caller
     */
    [[nodiscard]] AllocationStats GetGlobalStatsLocked() const;

    // Map: unique key -> AllocatorInfo
    std::unordered_map<std::string, AllocatorInfo> allocators_;

    // Mutex for thread-safety
    mutable std::mutex mutex_;
};

} // namespace comb::debug

#endif // COMB_MEM_DEBUG
