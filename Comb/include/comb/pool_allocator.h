#pragma once

#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>
#include <comb/platform.h>
#include <cstddef>
#include <algorithm>
#include <cstring>  // For memset
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
#endif

namespace comb
{
    /**
     * Pool allocator for fixed-size objects with free-list recycling
     *
     * Pre-allocates a pool of N objects of type T and manages them via free-list.
     * Provides ultra-fast O(1) allocation and deallocation with memory reuse.
     * Perfect for ECS entities, components, particles, and other fixed-size objects.
     *
     * Satisfies the comb::Allocator concept.
     *
     * Use cases:
     * - ECS entities and components (fixed types)
     * - Particle systems (allocate/free particles constantly)
     * - Object pools for frequently created/destroyed objects
     * - Game objects with predictable lifecycle
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────┐
     * │ [Object 0][Object 1][Object 2]...[Object N-1]      │
     * │    ↓         ↓         ↓                           │
     * │  free    in-use     free                           │
     * │    │                  │                            │
     * │    └──────────────────┘                            │
     * │  (free-list links free objects together)           │
     * └────────────────────────────────────────────────────┘
     *
     * Free-list mechanism:
     * - Each free slot stores a pointer to next free slot
     * - Allocation: pop from free-list head (O(1))
     * - Deallocation: push to free-list head (O(1))
     * - No fragmentation (all objects same size)
     *
     * Performance characteristics:
     * - Allocation: O(1) - pop from free-list
     * - Deallocation: O(1) - push to free-list
     * - Reset: O(1) - rebuild free-list
     * - Thread-safe: No (use per-thread pools or add mutex)
     * - Fragmentation: None (fixed-size objects)
     *
     * Limitations:
     * - Fixed object size (one pool per type)
     * - Fixed capacity (set at construction)
     * - Returns nullptr when pool exhausted (no hidden allocations)
     * - Not thread-safe (use PoolAllocator per thread or add synchronization)
     *
     * Example:
     * @code
     *   // Create pool for 1000 Enemy objects
     *   comb::PoolAllocator<Enemy> enemyPool{1000};
     *
     *   // Allocate enemy (O(1))
     *   Enemy* enemy = comb::New<Enemy>(enemyPool, health, position);
     *   if (!enemy) {
     *       // Pool exhausted - handle error
     *       return;
     *   }
     *
     *   // Use enemy...
     *
     *   // Free enemy - returns to free-list (O(1))
     *   comb::Delete(enemyPool, enemy);
     *
     *   // Memory immediately available for reuse!
     *   Enemy* another = comb::New<Enemy>(enemyPool); // Reuses freed memory
     *
     *   // Reset pool - frees all objects
     *   enemyPool.Reset();
     * @endcode
     */
    template<typename T>
    class PoolAllocator
    {
    public:
        /**
         * Construct pool allocator with capacity for N objects
         * @param capacity Number of objects to pre-allocate
         */
        explicit PoolAllocator(size_t capacity)
            : capacity_{capacity}
            , used_count_{0}
        {
            hive::Assert(capacity > 0, "Pool capacity must be > 0");

            constexpr size_t base_slot_size = (sizeof(T) > sizeof(void*)) ? sizeof(T) : sizeof(void*);

#if COMB_MEM_DEBUG
            // Front guard offset padded for proper user data alignment
            constexpr size_t user_align = (alignof(T) > alignof(void*)) ? alignof(T) : alignof(void*);
            constexpr size_t aligned_guard_front =
                (debug::GuardSize + user_align - 1) & ~(user_align - 1);
            // Slot: [front padding][user data][back guard], rounded for next slot alignment
            constexpr size_t slot_size_raw = aligned_guard_front + base_slot_size + debug::GuardSize;
            constexpr size_t slot_size = (slot_size_raw + user_align - 1) & ~(user_align - 1);
#else
            constexpr size_t slot_size = base_slot_size;
#endif

            total_size_ = capacity * slot_size;
            memory_block_ = AllocatePages(total_size_);
            hive::Assert(memory_block_ != nullptr, "Failed to allocate pool memory");

            Reset();

#if COMB_MEM_DEBUG
            registry_ = std::make_unique<debug::AllocationRegistry>();
            history_ = std::make_unique<debug::AllocationHistory>();
            debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), registry_.get());
#endif
        }

        ~PoolAllocator()
        {
#if COMB_MEM_DEBUG
            if (registry_)
            {
                if constexpr (debug::kLeakDetectionEnabled)
                {
                    registry_->ReportLeaks(GetName());
                }
                debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(registry_.get());
            }
#endif

            if (memory_block_)
            {
                FreePages(memory_block_, total_size_);
            }
        }

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        PoolAllocator(PoolAllocator&& other) noexcept
            : memory_block_{other.memory_block_}
            , free_list_head_{other.free_list_head_}
            , capacity_{other.capacity_}
            , used_count_{other.used_count_}
            , total_size_{other.total_size_}
#if COMB_MEM_DEBUG
            , registry_{std::move(other.registry_)}
            , history_{std::move(other.history_)}
#endif
        {
            other.memory_block_ = nullptr;
            other.free_list_head_ = nullptr;
            other.capacity_ = 0;
            other.used_count_ = 0;
            other.total_size_ = 0;
        }

        PoolAllocator& operator=(PoolAllocator&& other) noexcept
        {
            if (this != &other)
            {
#if COMB_MEM_DEBUG
                if (registry_)
                {
                    if constexpr (debug::kLeakDetectionEnabled)
                    {
                        registry_->ReportLeaks(GetName());
                    }
                    debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(registry_.get());
                }
#endif

                if (memory_block_)
                {
                    FreePages(memory_block_, total_size_);
                }

                memory_block_ = other.memory_block_;
                free_list_head_ = other.free_list_head_;
                capacity_ = other.capacity_;
                used_count_ = other.used_count_;
                total_size_ = other.total_size_;

#if COMB_MEM_DEBUG
                registry_ = std::move(other.registry_);
                history_ = std::move(other.history_);
#endif

                other.memory_block_ = nullptr;
                other.free_list_head_ = nullptr;
                other.capacity_ = 0;
                other.used_count_ = 0;
                other.total_size_ = 0;
            }
            return *this;
        }

        /**
         * Allocate memory for one object from the pool
         * Pops from free-list (O(1))
         *
         * @param size Number of bytes (must be <= sizeof(T), ignored)
         * @param alignment Required alignment (must be <= alignof(T), ignored)
         * @param tag Optional allocation tag for debugging (e.g., "Enemy #42")
         * @return Pointer to memory, or nullptr if pool exhausted
         *
         * Note: size and alignment parameters required by Allocator concept,
         * but ignored since pool only handles T-sized objects.
         * tag parameter is zero-cost when COMB_MEM_DEBUG=0.
         */
        [[nodiscard]] void* Allocate(size_t size, size_t alignment, const char* tag = nullptr)
        {
#if COMB_MEM_DEBUG
            void* ptr = AllocateDebug(size, alignment, tag);
#else
            (void)tag;  // Suppress unused warning

            hive::Assert(size <= sizeof(T), "PoolAllocator can only allocate sizeof(T) bytes");
            hive::Assert(alignment <= alignof(T), "PoolAllocator alignment limited to alignof(T)");

            if (!free_list_head_)
            {
                return nullptr;
            }

            void* ptr = free_list_head_;
            free_list_head_ = *static_cast<void**>(free_list_head_);
            ++used_count_;
#endif

            if (ptr)
            {
                HIVE_PROFILE_ALLOC(ptr, size, "PoolAllocator");
            }
            return ptr;
        }

        /**
         * Deallocate memory back to the pool
         * Pushes to free-list (O(1))
         *
         * @param ptr Pointer to deallocate (can be nullptr)
         *
         * IMPORTANT: Pointer must have been allocated from THIS pool.
         * No validation is performed - deallocating wrong pointer is undefined behavior.
         */
        void Deallocate(void* ptr)
        {
            if (!ptr)
                return;

            HIVE_PROFILE_FREE(ptr, "PoolAllocator");

#if COMB_MEM_DEBUG
            DeallocateDebug(ptr);
#else
            hive::Assert(used_count_ > 0, "Deallocate called more times than Allocate");

            *static_cast<void**>(ptr) = free_list_head_;
            free_list_head_ = ptr;
            --used_count_;
#endif
        }

        /**
         * Reset pool - marks all objects as free
         * Rebuilds free-list to initial state
         * Does NOT call destructors on objects!
         */
        void Reset()
        {
            constexpr size_t base_slot_size = (sizeof(T) > sizeof(void*)) ? sizeof(T) : sizeof(void*);

#if COMB_MEM_DEBUG
            constexpr size_t user_align = (alignof(T) > alignof(void*)) ? alignof(T) : alignof(void*);
            constexpr size_t aligned_guard_front =
                (debug::GuardSize + user_align - 1) & ~(user_align - 1);
            constexpr size_t slot_size_raw = aligned_guard_front + base_slot_size + debug::GuardSize;
            constexpr size_t slot_size = (slot_size_raw + user_align - 1) & ~(user_align - 1);
            constexpr size_t guard_offset = aligned_guard_front;
#else
            constexpr size_t slot_size = base_slot_size;
            constexpr size_t guard_offset = 0;
#endif

            auto* current = static_cast<std::byte*>(memory_block_) + guard_offset;
            free_list_head_ = current;

            for (size_t i = 0; i < capacity_ - 1; ++i)
            {
                auto* next = current + slot_size;
                *reinterpret_cast<void**>(current) = next;
                current = next;
            }

            *reinterpret_cast<void**>(current) = nullptr;

            used_count_ = 0;

#if COMB_MEM_DEBUG
            if (registry_)
            {
                registry_->Clear();
            }
#endif
        }

        /**
         * Get number of objects currently allocated
         * @return Number of objects in use
         */
        [[nodiscard]] size_t GetUsedMemory() const
        {
            return used_count_ * sizeof(T);
        }

        /**
         * Get total capacity of pool
         * @return Total bytes for all objects
         */
        [[nodiscard]] size_t GetTotalMemory() const
        {
            return capacity_ * sizeof(T);
        }

        /**
         * Get allocator name for debugging
         * @return "PoolAllocator"
         */
        [[nodiscard]] const char* GetName() const
        {
            return "PoolAllocator";
        }

        /**
         * Get pool capacity
         * @return Maximum number of objects
         */
        [[nodiscard]] size_t GetCapacity() const
        {
            return capacity_;
        }

        /**
         * Get number of objects currently in use
         * @return Number of allocated objects
         */
        [[nodiscard]] size_t GetUsedCount() const
        {
            return used_count_;
        }

        /**
         * Get number of free slots available
         * @return Number of objects that can still be allocated
         */
        [[nodiscard]] size_t GetFreeCount() const
        {
            return capacity_ - used_count_;
        }

    private:
        void* memory_block_{nullptr};      // Pre-allocated memory block
        void* free_list_head_{nullptr};    // Head of free-list
        size_t capacity_{0};               // Total number of objects
        size_t used_count_{0};             // Number of objects currently allocated
        size_t total_size_{0};             // Total size in bytes (for FreePages)

#if COMB_MEM_DEBUG
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> registry_;
        std::unique_ptr<debug::AllocationHistory> history_;
#endif
    };

    template<typename T>
    concept ValidPoolAllocator = Allocator<PoolAllocator<T>>;

#if COMB_MEM_DEBUG
    // ========================================================================
    // Debug Implementation (Template methods - must be in header)
    // ========================================================================

    template<typename T>
    void* PoolAllocator<T>::AllocateDebug(size_t size, size_t alignment, const char* tag)
    {
        using namespace debug;

        hive::Assert(size <= sizeof(T), "PoolAllocator can only allocate sizeof(T) bytes");
        hive::Assert(alignment <= alignof(T), "PoolAllocator alignment limited to alignof(T)");

        // Pool exhausted - check BEFORE adding guard bytes
        if (!free_list_head_)
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] [{}] Pool exhausted: size={}, capacity={}, tag={}",
                           GetName(), sizeof(T), capacity_, tag ? tag : "<no tag>");
            return nullptr;
        }

        // 1. Pop from free-list
        // In debug mode, free_list_head_ points to user data area (after guard)
        void* userPtr = free_list_head_;
        free_list_head_ = *static_cast<void**>(userPtr);
        ++used_count_;

        // 2. Add guard bytes (in-place within the pool slot)
        // Layout: [GUARD_FRONT (4B)][user data (sizeof(T))][GUARD_BACK (4B)]
        // Note: Pool slots are already sized for this in debug builds

        const size_t guardSize = sizeof(uint32_t);
        void* rawPtr = static_cast<std::byte*>(userPtr) - guardSize;

        WriteGuard(rawPtr);
        WriteGuard(static_cast<std::byte*>(userPtr) + sizeof(T));

        // 3. Initialize memory with pattern (detect uninitialized reads)
        if constexpr (kMemDebugEnabled)
        {
            std::memset(userPtr, AllocatedMemoryPattern, sizeof(T));
        }

        // 4. Register allocation
        AllocationInfo info{};
        info.address = userPtr;
        info.size = sizeof(T);
        info.alignment = alignof(T);
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

    template<typename T>
    void PoolAllocator<T>::DeallocateDebug(void* ptr)
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
                if (info->ReadGuardFront() != GuardMagic)
                {
                    hive::LogError(comb::LogCombRoot,
                                   "[MEM_DEBUG] [{}] Buffer UNDERRUN detected! Address: {}, Size: {}, Tag: {}",
                                   GetName(), ptr, info->size, info->GetTagOrDefault());
                    hive::Assert(false, "Buffer underrun detected");
                }

                if (info->ReadGuardBack() != GuardMagic)
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
        std::memset(ptr, FreedMemoryPattern, sizeof(T));
#endif

        // 4. Record deallocation in history
#if COMB_MEM_DEBUG_HISTORY
        history_->RecordDeallocation(ptr, sizeof(T));
#endif

        // 5. Unregister allocation
        registry_->UnregisterAllocation(ptr);

        // 6. Return to free-list
        // In debug mode, free-list stores pointers in user data area (not in guards)
        hive::Assert(used_count_ > 0, "Deallocate called more times than Allocate");

        // Push to free-list head (ptr is already userPtr in debug mode)
        *static_cast<void**>(ptr) = free_list_head_;
        free_list_head_ = ptr;
        --used_count_;
    }

#endif // COMB_MEM_DEBUG
}
