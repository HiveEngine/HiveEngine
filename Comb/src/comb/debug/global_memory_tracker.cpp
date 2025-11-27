/**
 * Global Memory Tracker Implementation
 *
 * Implementation of GlobalMemoryTracker methods.
 * Only compiled when COMB_MEM_DEBUG=1.
 */

#include <comb/debug/global_memory_tracker.h>

#if COMB_MEM_DEBUG

#include <sstream>
#include <iomanip>

namespace comb::debug
{

/**
 * Export all allocator stats to JSON string
 *
 * Generates JSON for external visualization tools.
 */
std::string GlobalMemoryTracker::ExportToJSON() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream json;
    json << std::fixed << std::setprecision(2);

    // Start JSON object
    json << "{\n";
    json << "  \"allocators\": [\n";

    // Export each allocator
    bool first = true;
    for (const auto& [key, info] : allocators_)
    {
        if (!first)
        {
            json << ",\n";
        }
        first = false;

        AllocationStats stats = info.registry->GetStats();

        json << "    {\n";
        json << "      \"name\": \"" << info.name << "\",\n";
        json << "      \"address\": \"" << reinterpret_cast<void*>(info.registry) << "\",\n";
        json << "      \"registrationTime\": " << info.registrationTime << ",\n";
        json << "      \"currentBytes\": " << stats.currentBytesUsed << ",\n";
        json << "      \"peakBytes\": " << stats.peakBytesUsed << ",\n";
        json << "      \"totalBytesAllocated\": " << stats.totalBytesAllocated << ",\n";
        json << "      \"currentAllocations\": " << stats.currentAllocations << ",\n";
        json << "      \"totalAllocations\": " << stats.totalAllocations << ",\n";
        json << "      \"totalDeallocations\": " << stats.totalDeallocations << ",\n";
        json << "      \"overheadBytes\": " << stats.overheadBytes << ",\n";
        json << "      \"overheadPercentage\": " << stats.GetOverheadPercentage() << ",\n";
        json << "      \"fragmentationRatio\": " << stats.GetFragmentationRatio() << ",\n";
        json << "      \"leakCount\": " << stats.GetLeakCount() << "\n";
        json << "    }";
    }

    json << "\n  ],\n";

    // Export global stats
    AllocationStats globalStats = GetGlobalStatsLocked();

    json << "  \"global\": {\n";
    json << "    \"currentBytes\": " << globalStats.currentBytesUsed << ",\n";
    json << "    \"peakBytes\": " << globalStats.peakBytesUsed << ",\n";
    json << "    \"totalBytesAllocated\": " << globalStats.totalBytesAllocated << ",\n";
    json << "    \"currentAllocations\": " << globalStats.currentAllocations << ",\n";
    json << "    \"totalAllocations\": " << globalStats.totalAllocations << ",\n";
    json << "    \"totalDeallocations\": " << globalStats.totalDeallocations << ",\n";
    json << "    \"overheadBytes\": " << globalStats.overheadBytes << ",\n";
    json << "    \"overheadPercentage\": " << globalStats.GetOverheadPercentage() << ",\n";
    json << "    \"fragmentationRatio\": " << globalStats.GetFragmentationRatio() << ",\n";
    json << "    \"leakCount\": " << globalStats.GetLeakCount() << ",\n";
    json << "    \"allocatorCount\": " << allocators_.size() << "\n";
    json << "  },\n";

    // Export metadata
    json << "  \"metadata\": {\n";
    json << "    \"exportTime\": " << GetTimestamp() << ",\n";
    json << "    \"memDebugEnabled\": true,\n";
    json << "    \"callstacksEnabled\": " << (COMB_MEM_DEBUG_CALLSTACKS ? "true" : "false") << "\n";
    json << "  }\n";

    json << "}\n";

    return json.str();
}

/**
 * Get global stats (without locking - internal use only)
 * Assumes mutex is already held by caller
 */
AllocationStats GlobalMemoryTracker::GetGlobalStatsLocked() const
{
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

        // Peak is max across all allocators
        if (allocStats.peakBytesUsed > globalStats.peakBytesUsed)
        {
            globalStats.peakBytesUsed = allocStats.peakBytesUsed;
        }
    }

    return globalStats;
}

} // namespace comb::debug

#endif // COMB_MEM_DEBUG
