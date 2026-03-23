#include <comb/default_allocator.h>

#include <cstdio>

namespace comb
{
    ModuleRegistry& ModuleRegistry::GetInstance()
    {
        static ModuleRegistry s_instance;
        return s_instance;
    }

    void ModuleRegistry::PrintStats() const
    {
        std::lock_guard<std::mutex> lock{m_mutex};

        std::printf("========== Module Memory Stats ==========\n");
        size_t totalUsed = 0;
        size_t totalCapacity = 0;

        for (size_t i = 0; i < m_count; ++i)
        {
            size_t used = m_entries[i].m_allocator->GetUsedMemory();
            size_t total = m_entries[i].m_allocator->GetTotalMemory();
            totalUsed += used;
            totalCapacity += total;

            double usedMB = static_cast<double>(used) / (1024.0 * 1024.0);
            double totalMB = static_cast<double>(total) / (1024.0 * 1024.0);
            double pct = total > 0 ? (static_cast<double>(used) / static_cast<double>(total)) * 100.0 : 0.0;

            std::printf("  %-20s %8.2f / %8.2f MB  (%5.1f%%)\n", m_entries[i].m_name, usedMB, totalMB, pct);
        }

        double totalUsedMB = static_cast<double>(totalUsed) / (1024.0 * 1024.0);
        double totalCapMB = static_cast<double>(totalCapacity) / (1024.0 * 1024.0);
        std::printf("  ----------------------------------------\n");
        std::printf("  %-20s %8.2f / %8.2f MB\n", "TOTAL", totalUsedMB, totalCapMB);
        std::printf("=========================================\n");
    }

    DefaultAllocator& GetDefaultAllocator()
    {
        static ModuleAllocator s_global{"Global", 32 * 1024 * 1024, 1024 * 1024 * 1024};
        return s_global.Get();
    }
} // namespace comb
