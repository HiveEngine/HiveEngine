#pragma once

#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <comb/utils.h>
#include <comb/platform.h>
#include <cstddef>
#include <array>
#include <memory>

// Memory debugging (zero overhead when disabled)
#if COMB_MEM_DEBUG
    #include <comb/debug/mem_debug_config.h>
    #include <comb/debug/allocation_registry.h>
    #include <comb/debug/allocation_history.h>
    #include <comb/debug/global_memory_tracker.h>
    #include <comb/debug/platform_utils.h>
    #include <hive/core/log.h>
    #include <comb/combmodule.h>
    #include <cstring>  // For memset
#endif

namespace comb
{
    /**
     * Slab allocator with multiple size classes and free-lists
     *
     * Manages multiple "slabs" (pools) of different object sizes.
     * Each slab is a PoolAllocator for a specific size class.
     * Allocations are routed to the smallest slab that can fit the request.
     *
     * Satisfies the comb::Allocator concept.
     *
     * Use cases:
     * - General-purpose allocation with known size distribution
     * - Multiple object types with different sizes
     * - Need fast allocation + deallocation with memory reuse
     * - Alternative to malloc with better performance
     *
     * Memory layout (example with 3 size classes: 32, 64, 128):
     * ┌──────────────────────────────────────────────────────┐
     * │ Slab 0 (32B):  [obj][obj][obj]...[obj] + free-list   │
     * │ Slab 1 (64B):  [obj][obj][obj]...[obj] + free-list   │
     * │ Slab 2 (128B): [obj][obj][obj]...[obj] + free-list   │
     * └──────────────────────────────────────────────────────┘
     *
     * Each slab operates independently with its own free-list.
     *
     * Performance characteristics:
     * - Allocation: O(1) - find slab O(K) + pop from free-list O(1), K = size classes
     * - Deallocation: O(1) - push to free-list
     * - Typical K = 8-16 size classes (small overhead)
     * - Thread-safe: No (use per-thread or add synchronization)
     * - Fragmentation: None (pre-allocated slabs)
     *
     * Limitations:
     * - Fixed size classes (set at compile time)
     * - Fixed capacity per slab
     * - Returns nullptr when slab exhausted (no hidden allocations)
     * - Not thread-safe by default
     *
     * Size class selection:
     * - Sizes are rounded up to next power of 2
     * - Common pattern: 16, 32, 64, 128, 256, 512, 1024, 2048
     * - Should cover 95% of your allocation sizes
     *
     * Example:
     * @code
     *   // Create slab allocator with 1000 objects per size class
     *   // Size classes: 32, 64, 128, 256, 512 bytes
     *   comb::SlabAllocator<1000, 32, 64, 128, 256, 512> slabs;
     *
     *   // Allocate 60 bytes - routed to 64-byte slab
     *   void* ptr1 = slabs.Allocate(60, 8);
     *   if (!ptr1) {
     *       // 64-byte slab exhausted
     *       return;
     *   }
     *
     *   // Allocate 200 bytes - routed to 256-byte slab
     *   void* ptr2 = slabs.Allocate(200, 8);
     *
     *   // Free memory - returns to appropriate free-list
     *   slabs.Deallocate(ptr1);  // Returns to 64-byte slab
     *   slabs.Deallocate(ptr2);  // Returns to 256-byte slab
     *
     *   // Memory immediately available for reuse
     *   void* ptr3 = slabs.Allocate(60, 8);  // Reuses ptr1's slot
     *
     *   // Reset all slabs at once
     *   slabs.Reset();
     * @endcode
     */
    template<size_t ObjectsPerSlab, size_t... SizeClasses>
    class SlabAllocator
    {
        static_assert(sizeof...(SizeClasses) > 0, "Must provide at least one size class");
        static_assert(ObjectsPerSlab > 0, "Must allocate at least one object per slab");

    private:
        // Size classes rounded to powers of 2 and sorted
        static constexpr auto sizes_ = MakeArray(NextPowerOfTwo(SizeClasses)...);
        static_assert(IsSorted(sizes_), "Size classes must be sorted");

        static constexpr size_t NumSlabs = sizeof...(SizeClasses);

        // Each slab: memory block + free-list head + usage counter
        struct Slab
        {
            void* memory_block{nullptr};
            void* free_list_head{nullptr};
            size_t used_count{0};
            size_t slot_size{0};
            size_t total_size{0};
            size_t free_list_offset_{0};  // Track offset for Reset()
            size_t user_size_{0};  // Track user-visible size (without guard bytes)

            void Initialize(size_t size, size_t free_list_offset = 0)
            {
                slot_size = size;
                total_size = ObjectsPerSlab * slot_size;
                free_list_offset_ = free_list_offset;

                // Calculate user size (slot_size minus guard bytes in debug mode)
#if COMB_MEM_DEBUG
                user_size_ = size - debug::TotalGuardSize;
#else
                user_size_ = size;
#endif

                // Allocate memory from OS
                memory_block = AllocatePages(total_size);
                hive::Assert(memory_block != nullptr, "Failed to allocate slab memory");

                // Build free-list (with optional offset for debug guard bytes)
                RebuildFreeList(free_list_offset);
            }

            void Destroy()
            {
                if (memory_block)
                {
                    FreePages(memory_block, total_size);
                    memory_block = nullptr;
                    free_list_head = nullptr;
                }
            }

            void RebuildFreeList(size_t free_list_offset)
            {
                // In debug mode, free_list_offset skips the guard bytes at the start of each slot
                char* current = static_cast<char*>(memory_block) + free_list_offset;
                free_list_head = current;

                for (size_t i = 0; i < ObjectsPerSlab - 1; ++i)
                {
                    char* next = current + slot_size;
                    *reinterpret_cast<void**>(current) = next;
                    current = next;
                }

                *reinterpret_cast<void**>(current) = nullptr;
                used_count = 0;
            }

            void RebuildFreeList()
            {
                // Use stored offset from Initialize()
                RebuildFreeList(free_list_offset_);
            }

            void* Allocate()
            {
                if (!free_list_head)
                {
                    return nullptr;
                }

                void* ptr = free_list_head;
                free_list_head = *static_cast<void**>(free_list_head);
                ++used_count;
                return ptr;
            }

            void Deallocate(void* ptr)
            {
                if (!ptr)
                    return;

                hive::Assert(used_count > 0, "Deallocate called more than Allocate");

                *static_cast<void**>(ptr) = free_list_head;
                free_list_head = ptr;
                --used_count;
            }

            bool Contains(void* ptr) const
            {
                if (!ptr || !memory_block)
                    return false;

                const char* start = static_cast<const char*>(memory_block);
                const char* end = start + (ObjectsPerSlab * slot_size);
                const char* p = static_cast<const char*>(ptr);

                return p >= start && p < end;
            }

            size_t GetUsedMemory() const
            {
                // Return user-visible memory (excluding guard bytes)
                return used_count * user_size_;
            }

            size_t GetTotalMemory() const
            {
                // Return user-visible capacity (excluding guard bytes)
                return ObjectsPerSlab * user_size_;
            }

            size_t GetFreeCount() const
            {
                return ObjectsPerSlab - used_count;
            }
        };

        std::array<Slab, NumSlabs> slabs_{};

        // Find slab index for given size
        constexpr size_t FindSlabIndex(size_t size) const
        {
            for (size_t i = 0; i < sizes_.size(); ++i)
            {
                if (size <= sizes_[i])
                {
                    return i;
                }
            }
            return NumSlabs;  // No slab large enough
        }

    public:
        SlabAllocator(const SlabAllocator&) = delete;
        SlabAllocator& operator=(const SlabAllocator&) = delete;

        /**
         * Construct slab allocator and initialize all slabs
         */
        SlabAllocator()
        {
            for (size_t i = 0; i < NumSlabs; ++i)
            {
#if COMB_MEM_DEBUG
                // Debug mode: slots need extra space for guard bytes
                // Free-list starts after the front guard
                slabs_[i].Initialize(sizes_[i] + debug::TotalGuardSize, debug::GuardSize);
#else
                slabs_[i].Initialize(sizes_[i]);
#endif
            }

#if COMB_MEM_DEBUG
            // Create debug tracking objects
            registry_ = std::make_unique<debug::AllocationRegistry>();
            history_ = std::make_unique<debug::AllocationHistory>();

            // Register with global tracker
            debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), registry_.get());
#endif
        }

        /**
         * Destructor - frees all slab memory
         */
        ~SlabAllocator()
        {
#if COMB_MEM_DEBUG
            if (registry_)
            {
                // Report leaks before destruction
                if constexpr (debug::kLeakDetectionEnabled)
                {
                    registry_->ReportLeaks(GetName());
                }

                // Unregister from global tracker
                debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(registry_.get());
            }
#endif

            for (auto& slab : slabs_)
            {
                slab.Destroy();
            }
        }

        /**
         * Move constructor
         *
         * Transfers ownership of memory blocks and debug tracking to new allocator.
         * All allocations made before move can still be deallocated from moved-to allocator.
         */
        SlabAllocator(SlabAllocator&& other) noexcept
            : slabs_{std::move(other.slabs_)}
#if COMB_MEM_DEBUG
            , registry_{std::move(other.registry_)}
            , history_{std::move(other.history_)}
#endif
        {
            // Note: registry_ and history_ are automatically moved via unique_ptr
            // No need to update global tracker - it still points to the same registry object

            // Invalidate other
            for (auto& slab : other.slabs_)
            {
                slab.memory_block = nullptr;
                slab.free_list_head = nullptr;
                slab.used_count = 0;
            }
        }

        SlabAllocator& operator=(SlabAllocator&& other) noexcept
        {
            if (this != &other)
            {
#if COMB_MEM_DEBUG
                if (registry_)
                {
                    // Report leaks for our current allocations before we destroy them
                    if constexpr (debug::kLeakDetectionEnabled)
                    {
                        registry_->ReportLeaks(GetName());
                    }

                    // Unregister from global tracker
                    debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(registry_.get());
                }
#endif

                // Destroy our slabs
                for (auto& slab : slabs_)
                {
                    slab.Destroy();
                }

                // Move from other
                slabs_ = std::move(other.slabs_);

#if COMB_MEM_DEBUG
                // Move the debug tracking objects
                registry_ = std::move(other.registry_);
                history_ = std::move(other.history_);
                // No need to re-register - global tracker still points to same registry object
#endif

                // Invalidate other
                for (auto& slab : other.slabs_)
                {
                    slab.memory_block = nullptr;
                    slab.free_list_head = nullptr;
                    slab.used_count = 0;
                }
            }
            return *this;
        }

        /**
         * Allocate memory from appropriate slab
         *
         * @param size Number of bytes to allocate
         * @param alignment Required alignment (must be <= alignof(std::max_align_t))
         * @param tag Optional allocation tag for debugging (e.g., "Entity #42")
         * @return Pointer to allocated memory, or nullptr if:
         *         - No slab can fit the requested size
         *         - Appropriate slab is exhausted
         *
         * Note: tag parameter is zero-cost when COMB_MEM_DEBUG=0
         *
         * IMPORTANT: Does NOT fallback to operator new. Returns nullptr when out of memory.
         */
        [[nodiscard]] void* Allocate(size_t size, size_t alignment, const char* tag = nullptr)
        {
#if COMB_MEM_DEBUG
            return AllocateDebug(size, alignment, tag);
#else
            (void)tag;  // Suppress unused warning

            hive::Assert(alignment <= alignof(std::max_align_t),
                         "SlabAllocator alignment limited to max_align_t");

            const size_t slab_index = FindSlabIndex(size);

            // No slab large enough
            if (slab_index >= NumSlabs)
            {
                return nullptr;
            }

            return slabs_[slab_index].Allocate();
#endif
        }

        /**
         * Deallocate memory back to appropriate slab
         *
         * @param ptr Pointer to deallocate (can be nullptr)
         *
         * IMPORTANT: Pointer must have been allocated from THIS allocator.
         * Finds which slab owns the pointer and returns it to that slab's free-list.
         */
        void Deallocate(void* ptr)
        {
#if COMB_MEM_DEBUG
            DeallocateDebug(ptr);
#else
            if (!ptr)
                return;

            // Find which slab owns this pointer
            for (auto& slab : slabs_)
            {
                if (slab.Contains(ptr))
                {
                    slab.Deallocate(ptr);
                    return;
                }
            }

            // Pointer not from any slab - error
            hive::Assert(false, "Pointer not allocated from this SlabAllocator");
#endif
        }

        /**
         * Reset all slabs - marks all memory as free
         * Rebuilds free-lists for all slabs
         */
        void Reset()
        {
            for (auto& slab : slabs_)
            {
                slab.RebuildFreeList();
            }

#if COMB_MEM_DEBUG
            // Clear debug tracking registry
            if (registry_)
            {
                registry_->Clear();
            }
#endif
        }

        /**
         * Get total bytes currently allocated across all slabs
         */
        [[nodiscard]] size_t GetUsedMemory() const
        {
            size_t total = 0;
            for (const auto& slab : slabs_)
            {
                total += slab.GetUsedMemory();
            }
            return total;
        }

        /**
         * Get total capacity across all slabs
         */
        [[nodiscard]] size_t GetTotalMemory() const
        {
            size_t total = 0;
            for (const auto& slab : slabs_)
            {
                total += slab.GetTotalMemory();
            }
            return total;
        }

        /**
         * Get allocator name for debugging
         */
        [[nodiscard]] const char* GetName() const
        {
            return "SlabAllocator";
        }

        /**
         * Get number of size classes
         */
        [[nodiscard]] constexpr size_t GetSlabCount() const
        {
            return NumSlabs;
        }

        /**
         * Get size classes array
         */
        [[nodiscard]] constexpr auto GetSizeClasses() const
        {
            return sizes_;
        }

        /**
         * Get usage stats for a specific slab
         */
        [[nodiscard]] size_t GetSlabUsedCount(size_t slab_index) const
        {
            hive::Assert(slab_index < NumSlabs, "Slab index out of range");
            return slabs_[slab_index].used_count;
        }

        /**
         * Get free count for a specific slab
         */
        [[nodiscard]] size_t GetSlabFreeCount(size_t slab_index) const
        {
            hive::Assert(slab_index < NumSlabs, "Slab index out of range");
            return slabs_[slab_index].GetFreeCount();
        }

#if COMB_MEM_DEBUG
    private:
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> registry_;
        std::unique_ptr<debug::AllocationHistory> history_;
#endif
    };

    template<size_t N, size_t... Sizes>
    concept ValidSlabAllocator = Allocator<SlabAllocator<N, Sizes...>>;

#if COMB_MEM_DEBUG
    // ========================================================================
    // Debug Implementation (Template methods - must be in header)
    // ========================================================================

    template<size_t ObjectsPerSlab, size_t... SizeClasses>
    void* SlabAllocator<ObjectsPerSlab, SizeClasses...>::AllocateDebug(size_t size, size_t alignment, const char* tag)
    {
        using namespace debug;

        hive::Assert(alignment <= alignof(std::max_align_t),
                     "SlabAllocator alignment limited to max_align_t");

        const size_t slab_index = FindSlabIndex(size);

        // No slab large enough - check BEFORE adding guard bytes
        if (slab_index >= NumSlabs)
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] [{}] No slab can fit size={}, max_size={}, tag={}",
                           GetName(), size, sizes_[NumSlabs - 1], tag ? tag : "<no tag>");
            return nullptr;
        }

        // Get actual slot size for this slab
        const size_t slotSize = sizes_[slab_index];

        // 1. Pop from slab free-list
        // In debug mode, Allocate() returns a pointer after the guard front
        void* userPtr = slabs_[slab_index].Allocate();
        if (!userPtr)
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] [{}] Slab {} (size={}) exhausted: requested size={}, tag={}",
                           GetName(), slab_index, slotSize, size, tag ? tag : "<no tag>");
            return nullptr;
        }

        // 2. Add guard bytes in-place within the slab slot
        // Layout: [GUARD_FRONT (4B)][user data (size)][GUARD_BACK (4B)]
        const size_t guardSize = sizeof(uint32_t);
        void* rawPtr = static_cast<std::byte*>(userPtr) - guardSize;

        auto* guardFront = static_cast<uint32_t*>(rawPtr);
        *guardFront = GuardMagic;

        auto* guardBack = reinterpret_cast<uint32_t*>(
            static_cast<std::byte*>(userPtr) + size
        );
        *guardBack = GuardMagic;

        // 3. Initialize memory with pattern (detect uninitialized reads)
        if constexpr (kMemDebugEnabled)
        {
            std::memset(userPtr, AllocatedMemoryPattern, size);
        }

        // 4. Register allocation
        AllocationInfo info{};
        info.address = userPtr;
        info.size = size;
        info.alignment = alignment;
        info.timestamp = GetTimestamp();
        info.tag = tag;
        info.allocationId = registry_->GetNextAllocationId();
        info.threadId = GetThreadId();

#if COMB_MEM_DEBUG_CALLSTACKS
        CaptureCallstack(info.callstack, info.callstackDepth);
#endif

        registry_->RegisterAllocation(info);

        // 5. Record in history
#if COMB_MEM_DEBUG_HISTORY
        history_->RecordAllocation(info);
#endif

        return userPtr;
    }

    template<size_t ObjectsPerSlab, size_t... SizeClasses>
    void SlabAllocator<ObjectsPerSlab, SizeClasses...>::DeallocateDebug(void* ptr)
    {
        using namespace debug;

        if (!ptr) return;

        // 1. Find allocation info
        auto* info = registry_->FindAllocation(ptr);
        if (!info)
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] [{}] Double-free or invalid pointer detected! Address: {}",
                           GetName(), ptr);
            hive::Assert(false, "Double-free or invalid pointer (not found in registry)");
            return;
        }

        // 2. Check guard bytes
        if constexpr (kMemDebugEnabled)
        {
            if (!info->CheckGuards())
            {
                auto* guardFront = info->GetGuardFront();
                auto* guardBack = info->GetGuardBack();

                if (*guardFront != GuardMagic)
                {
                    hive::LogError(comb::LogCombRoot,
                                   "[MEM_DEBUG] [{}] Buffer UNDERRUN detected! Address: {}, Size: {}, Tag: {}",
                                   GetName(), ptr, info->size, info->GetTagOrDefault());
                    hive::Assert(false, "Buffer underrun detected");
                }

                if (*guardBack != GuardMagic)
                {
                    hive::LogError(comb::LogCombRoot,
                                   "[MEM_DEBUG] [{}] Buffer OVERRUN detected! Address: {}, Size: {}, Tag: {}",
                                   GetName(), ptr, info->size, info->GetTagOrDefault());
                    hive::Assert(false, "Buffer overrun detected");
                }
            }
        }

        // 3. Fill with freed pattern (detect use-after-free)
#if COMB_MEM_DEBUG_USE_AFTER_FREE
        std::memset(ptr, FreedMemoryPattern, info->size);
#endif

        // 4. Record deallocation in history
#if COMB_MEM_DEBUG_HISTORY
        history_->RecordDeallocation(ptr, info->size);
#endif

        // 5. Unregister allocation
        registry_->UnregisterAllocation(ptr);

        // 6. Return to slab free-list
        // In debug mode, the slab's free-list stores pointers in user data area (after guard)
        // So we deallocate using ptr directly (which is userPtr)
        for (auto& slab : slabs_)
        {
            // Check if this slab owns the memory block containing ptr
            if (slab.Contains(ptr))
            {
                slab.Deallocate(ptr);
                return;
            }
        }

        // Should never happen - ptr should be from one of our slabs
        hive::Assert(false, "Internal error: ptr not found in any slab");
    }

#endif // COMB_MEM_DEBUG
}
