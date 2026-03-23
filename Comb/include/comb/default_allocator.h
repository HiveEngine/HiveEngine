#pragma once

#include <hive/hive_config.h>

#include <comb/chained_buddy_allocator.h>
#include <comb/thread_safe_allocator.h>

#include <mutex>

namespace comb
{
    // Default allocator: ThreadSafeAllocator<ChainedBuddyAllocator>.
    // Growable general-purpose allocator with mutex-protected access.
    using DefaultAllocator = ThreadSafeAllocator<ChainedBuddyAllocator>;

    // Forward declaration
    class ModuleAllocator;

    /**
     * Registry that tracks all ModuleAllocators for memory stats
     *
     * Works in all build modes (not just COMB_MEM_DEBUG).
     * Thread-safe singleton.
     */
    class HIVE_API ModuleRegistry
    {
    public:
        static constexpr size_t kMaxModules = 64;

        struct Entry
        {
            const char* m_name;
            ModuleAllocator* m_allocator;
        };

        static ModuleRegistry& GetInstance();

        void Register(const char* name, ModuleAllocator* alloc)
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            if (m_count < kMaxModules)
            {
                m_entries[m_count] = {name, alloc};
                ++m_count;
            }
        }

        void Unregister(ModuleAllocator* alloc)
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            for (size_t i = 0; i < m_count; ++i)
            {
                if (m_entries[i].m_allocator == alloc)
                {
                    m_entries[i] = m_entries[m_count - 1];
                    --m_count;
                    return;
                }
            }
        }

        void PrintStats() const;

        [[nodiscard]] size_t GetCount() const noexcept
        {
            return m_count;
        }
        [[nodiscard]] const Entry& GetEntry(size_t index) const noexcept
        {
            return m_entries[index];
        }

    private:
        ModuleRegistry() = default;

        Entry m_entries[kMaxModules]{};
        size_t m_count{0};
        mutable std::mutex m_mutex;
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
    class HIVE_API ModuleAllocator
    {
    public:
        ModuleAllocator(const char* name, size_t capacity)
            : m_name{name}
            , m_chained{capacity, capacity, name}
            , m_allocator{m_chained}
        {
            ModuleRegistry::GetInstance().Register(m_name, this);
        }

        ModuleAllocator(const char* name, size_t blockSize, size_t hardCap)
            : m_name{name}
            , m_chained{blockSize, hardCap, name}
            , m_allocator{m_chained}
        {
            ModuleRegistry::GetInstance().Register(m_name, this);
        }

        ~ModuleAllocator()
        {
            ModuleRegistry::GetInstance().Unregister(this);
        }

        ModuleAllocator(const ModuleAllocator&) = delete;
        ModuleAllocator& operator=(const ModuleAllocator&) = delete;
        ModuleAllocator(ModuleAllocator&&) = delete;
        ModuleAllocator& operator=(ModuleAllocator&&) = delete;

        [[nodiscard]] DefaultAllocator& Get() noexcept
        {
            return m_allocator;
        }
        [[nodiscard]] const DefaultAllocator& Get() const noexcept
        {
            return m_allocator;
        }

        [[nodiscard]] ChainedBuddyAllocator& GetUnderlying() noexcept
        {
            return m_chained;
        }
        [[nodiscard]] const char* GetName() const noexcept
        {
            return m_name;
        }

        [[nodiscard]] size_t GetUsedMemory() const
        {
            return m_allocator.GetUsedMemory();
        }
        [[nodiscard]] size_t GetTotalMemory() const
        {
            return m_allocator.GetTotalMemory();
        }

    private:
        const char* m_name;
        ChainedBuddyAllocator m_chained;
        DefaultAllocator m_allocator;
    };


    /**
     * Get the global default allocator instance (Meyers singleton)
     *
     * Used as fallback when no module allocator is provided.
     *
     * @return Reference to the global default allocator
     */
    HIVE_API DefaultAllocator& GetDefaultAllocator();

    static_assert(Allocator<DefaultAllocator>, "DefaultAllocator must satisfy comb::Allocator concept");
} // namespace comb
