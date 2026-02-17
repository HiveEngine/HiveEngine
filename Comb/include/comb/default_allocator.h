#pragma once

#include <comb/buddy_allocator.h>
#include <comb/thread_safe_allocator.h>

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

    /**
     * Per-module allocator that bundles BuddyAllocator + ThreadSafeAllocator
     *
     * Each module/system should create its own ModuleAllocator to isolate
     * memory usage. Implicitly converts to DefaultAllocator& so it can be
     * passed directly to any container constructor.
     *
     * Example:
     * @code
     *   // In Queen module (e.g. queen_allocator.h)
     *   namespace queen {
     *       inline comb::DefaultAllocator& GetAllocator() {
     *           static comb::ModuleAllocator alloc{16 * 1024 * 1024};  // 16 MB
     *           return alloc.Get();
     *       }
     *   }
     *
     *   // Usage in Queen code:
     *   wax::Vector<int> entities{queen::GetAllocator()};
     *   wax::HashMap<int, int> lookup{queen::GetAllocator()};
     * @endcode
     */
    class ModuleAllocator
    {
    public:
        explicit ModuleAllocator(size_t capacity)
            : buddy_{capacity}
            , allocator_{buddy_}
        {}

        ModuleAllocator(const ModuleAllocator&) = delete;
        ModuleAllocator& operator=(const ModuleAllocator&) = delete;
        ModuleAllocator(ModuleAllocator&&) = delete;
        ModuleAllocator& operator=(ModuleAllocator&&) = delete;

        [[nodiscard]] DefaultAllocator& Get() noexcept { return allocator_; }
        [[nodiscard]] const DefaultAllocator& Get() const noexcept { return allocator_; }

        [[nodiscard]] BuddyAllocator& GetUnderlying() noexcept { return buddy_; }

    private:
        BuddyAllocator buddy_;
        DefaultAllocator allocator_;
    };

    /**
     * Get the global default allocator instance (Meyers singleton)
     *
     * Used as fallback when no module allocator is provided.
     * Thread-safe initialization guaranteed by C++11 static local rules.
     *
     * @return Reference to the global default allocator
     */
    inline DefaultAllocator& GetDefaultAllocator()
    {
        static ModuleAllocator global{32 * 1024 * 1024};  // 32 MB
        return global.Get();
    }

    static_assert(Allocator<DefaultAllocator>,
                  "DefaultAllocator must satisfy comb::Allocator concept");
}
