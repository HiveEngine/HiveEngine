/**
 * Global Memory Tracker Implementation
 *
 * Implementation of GlobalMemoryTracker methods.
 * Only compiled when COMB_MEM_DEBUG=1.
 */

#include <comb/debug/global_memory_tracker.h>

#if COMB_MEM_DEBUG

#include <iomanip>
#include <sstream>

namespace comb::debug
{

    /**
     * Export all allocator stats to JSON string
     *
     * Generates JSON for external visualization tools.
     */
    std::string GlobalMemoryTracker::ExportToJSON() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::ostringstream json;
        json << std::fixed << std::setprecision(2);

        // Start JSON object
        json << "{\n";
        json << "  \"allocators\": [\n";

        // Export each allocator
        bool first = true;
        for (const auto& [key, info] : m_allocators)
        {
            if (!first)
            {
                json << ",\n";
            }
            first = false;

            AllocationStats stats = info.m_registry->GetStats();

            json << "    {\n";
            json << "      \"name\": \"" << info.m_name << "\",\n";
            json << "      \"address\": \"" << reinterpret_cast<void*>(info.m_registry) << "\",\n";
            json << "      \"registrationTime\": " << info.m_registrationTime << ",\n";
            json << "      \"currentBytes\": " << stats.m_currentBytesUsed << ",\n";
            json << "      \"peakBytes\": " << stats.m_peakBytesUsed << ",\n";
            json << "      \"totalBytesAllocated\": " << stats.m_totalBytesAllocated << ",\n";
            json << "      \"currentAllocations\": " << stats.m_currentAllocations << ",\n";
            json << "      \"totalAllocations\": " << stats.m_totalAllocations << ",\n";
            json << "      \"totalDeallocations\": " << stats.m_totalDeallocations << ",\n";
            json << "      \"overheadBytes\": " << stats.m_overheadBytes << ",\n";
            json << "      \"overheadPercentage\": " << stats.GetOverheadPercentage() << ",\n";
            json << "      \"fragmentationRatio\": " << stats.GetFragmentationRatio() << ",\n";
            json << "      \"leakCount\": " << stats.GetLeakCount() << "\n";
            json << "    }";
        }

        json << "\n  ],\n";

        // Export global stats
        AllocationStats globalStats = GetGlobalStatsLocked();

        json << "  \"global\": {\n";
        json << "    \"currentBytes\": " << globalStats.m_currentBytesUsed << ",\n";
        json << "    \"peakBytes\": " << globalStats.m_peakBytesUsed << ",\n";
        json << "    \"totalBytesAllocated\": " << globalStats.m_totalBytesAllocated << ",\n";
        json << "    \"currentAllocations\": " << globalStats.m_currentAllocations << ",\n";
        json << "    \"totalAllocations\": " << globalStats.m_totalAllocations << ",\n";
        json << "    \"totalDeallocations\": " << globalStats.m_totalDeallocations << ",\n";
        json << "    \"overheadBytes\": " << globalStats.m_overheadBytes << ",\n";
        json << "    \"overheadPercentage\": " << globalStats.GetOverheadPercentage() << ",\n";
        json << "    \"fragmentationRatio\": " << globalStats.GetFragmentationRatio() << ",\n";
        json << "    \"leakCount\": " << globalStats.GetLeakCount() << ",\n";
        json << "    \"allocatorCount\": " << m_allocators.size() << "\n";
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

        for (const auto& [key, info] : m_allocators)
        {
            AllocationStats allocStats = info.m_registry->GetStats();

            globalStats.m_totalAllocations += allocStats.m_totalAllocations;
            globalStats.m_totalDeallocations += allocStats.m_totalDeallocations;
            globalStats.m_currentAllocations += allocStats.m_currentAllocations;
            globalStats.m_currentBytesUsed += allocStats.m_currentBytesUsed;
            globalStats.m_totalBytesAllocated += allocStats.m_totalBytesAllocated;
            globalStats.m_overheadBytes += allocStats.m_overheadBytes;

            // Peak is max across all allocators
            if (allocStats.m_peakBytesUsed > globalStats.m_peakBytesUsed)
            {
                globalStats.m_peakBytesUsed = allocStats.m_peakBytesUsed;
            }
        }

        return globalStats;
    }

} // namespace comb::debug

#endif // COMB_MEM_DEBUG
