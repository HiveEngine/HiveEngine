#pragma once

#include <comb/buddy_allocator.h>
#include <comb/thread_safe_allocator.h>
#include <mutex>
#include <cstdio>

namespace comb
{
    /**
     * Default allocator type used by containers when no allocator is specified
     *
     * ThreadSafeAllocator<BuddyAllocator> provides:
     * - General-purpose allocation (BuddyAllocator)
     * - Thread safety via mutex (ThreadSafeAllocator wrapper)
     *
     * For performance-critical code, prefer explicit allocators (LinearAllocator
     * for frame data, PoolAllocator for fixed-size objects, etc.)
     */
    using DefaultAllocator = ThreadSafeAllocator<BuddyAllocator>;

    // Forward declaration
    class ModuleAllocator;

    /**
     * Registry that tracks all ModuleAllocators for memory stats
     *
     * Works in all build modes (not just COMB_MEM_DEBUG).
     * Thread-safe singleton.
     */
    class ModuleRegistry
    {
    public:
        static constexpr size_t kMaxModules = 64;

        struct Entry
        {
            const char* name;
            ModuleAllocator* allocator;
        };

        static ModuleRegistry& GetInstance()
        {
            static ModuleRegistry instance;
            return instance;
        }

        void Register(const char* name, ModuleAllocator* alloc)
        {
            std::lock_guard<std::mutex> lock{mutex_};
            if (count_ < kMaxModules)
            {
                entries_[count_] = {name, alloc};
                ++count_;
            }
        }

        void Unregister(ModuleAllocator* alloc)
        {
            std::lock_guard<std::mutex> lock{mutex_};
            for (size_t i = 0; i < count_; ++i)
            {
                if (entries_[i].allocator == alloc)
                {
                    entries_[i] = entries_[count_ - 1];
                    --count_;
                    return;
                }
            }
        }

        void PrintStats() const;

        [[nodiscard]] size_t GetCount() const noexcept { return count_; }
        [[nodiscard]] const Entry& GetEntry(size_t index) const noexcept { return entries_[index]; }

    private:
        ModuleRegistry() = default;

        Entry entries_[kMaxModules]{};
        size_t count_{0};
        mutable std::mutex mutex_;
    };

    /**
     * Per-module allocator that bundles BuddyAllocator + ThreadSafeAllocator
     *
     * Each module/system should create its own ModuleAllocator to isolate
     * memory usage. Auto-registers with ModuleRegistry for stats tracking.
     *
     * Example:
     * @code
     *   // In Queen module
     *   namespace queen {
     *       inline comb::DefaultAllocator& GetAllocator() {
     *           static comb::ModuleAllocator alloc{"Queen", 16 * 1024 * 1024};
     *           return alloc.Get();
     *       }
     *   }
     *
     *   // Usage:
     *   wax::Vector<int> entities{queen::GetAllocator()};
     *
     *   // Print all module stats:
     *   comb::ModuleRegistry::GetInstance().PrintStats();
     * @endcode
     */
    class ModuleAllocator
    {
    public:
        ModuleAllocator(const char* name, size_t capacity)
            : name_{name}
            , buddy_{capacity}
            , allocator_{buddy_}
        {
            ModuleRegistry::GetInstance().Register(name_, this);
        }

        ~ModuleAllocator()
        {
            ModuleRegistry::GetInstance().Unregister(this);
        }

        ModuleAllocator(const ModuleAllocator&) = delete;
        ModuleAllocator& operator=(const ModuleAllocator&) = delete;
        ModuleAllocator(ModuleAllocator&&) = delete;
        ModuleAllocator& operator=(ModuleAllocator&&) = delete;

        [[nodiscard]] DefaultAllocator& Get() noexcept { return allocator_; }
        [[nodiscard]] const DefaultAllocator& Get() const noexcept { return allocator_; }

        [[nodiscard]] BuddyAllocator& GetUnderlying() noexcept { return buddy_; }
        [[nodiscard]] const char* GetName() const noexcept { return name_; }

        [[nodiscard]] size_t GetUsedMemory() const { return allocator_.GetUsedMemory(); }
        [[nodiscard]] size_t GetTotalMemory() const { return allocator_.GetTotalMemory(); }

    private:
        const char* name_;
        BuddyAllocator buddy_;
        DefaultAllocator allocator_;
    };

    /**
     * Print memory stats for all registered modules
     */
    inline void ModuleRegistry::PrintStats() const
    {
        std::lock_guard<std::mutex> lock{mutex_};

        std::printf("========== Module Memory Stats ==========\n");
        size_t totalUsed = 0;
        size_t totalCapacity = 0;

        for (size_t i = 0; i < count_; ++i)
        {
            size_t used = entries_[i].allocator->GetUsedMemory();
            size_t total = entries_[i].allocator->GetTotalMemory();
            totalUsed += used;
            totalCapacity += total;

            double usedMB = static_cast<double>(used) / (1024.0 * 1024.0);
            double totalMB = static_cast<double>(total) / (1024.0 * 1024.0);
            double pct = total > 0 ? (static_cast<double>(used) / static_cast<double>(total)) * 100.0 : 0.0;

            std::printf("  %-20s %8.2f / %8.2f MB  (%5.1f%%)\n",
                        entries_[i].name, usedMB, totalMB, pct);
        }

        double totalUsedMB = static_cast<double>(totalUsed) / (1024.0 * 1024.0);
        double totalCapMB = static_cast<double>(totalCapacity) / (1024.0 * 1024.0);
        std::printf("  ----------------------------------------\n");
        std::printf("  %-20s %8.2f / %8.2f MB\n", "TOTAL", totalUsedMB, totalCapMB);
        std::printf("=========================================\n");
    }

    /**
     * Get the global default allocator instance (Meyers singleton)
     *
     * Used as fallback when no module allocator is provided.
     *
     * @return Reference to the global default allocator
     */
    inline DefaultAllocator& GetDefaultAllocator()
    {
        static ModuleAllocator global{"Global", 32 * 1024 * 1024};  // 32 MB
        return global.Get();
    }

    static_assert(Allocator<DefaultAllocator>,
                  "DefaultAllocator must satisfy comb::Allocator concept");
}
