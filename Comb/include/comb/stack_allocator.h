#pragma once

#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <comb/utils.h>
#include <comb/platform.h>
#include <cstddef>
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
     * Stack allocator with LIFO (Last-In-First-Out) deallocation using markers
     *
     * Similar to LinearAllocator but supports scoped deallocations via markers.
     * Allocates memory sequentially by bumping a pointer forward.
     * Deallocations are done by restoring to a saved marker position.
     *
     * Satisfies the comb::Allocator concept.
     *
     * Use cases:
     * - Scoped temporary allocations (nested function calls)
     * - Recursive algorithms with cleanup at each level
     * - Frame temps with multiple Reset points
     * - Per-scope resource management
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ [Alloc 1][Alloc 2][Alloc 3]...[Alloc N]  [Free Space]      │
     * │  ←──────── Used Memory ────────────→                       │
     * └────────────────────────────────────────────────────────────┘
     *  ↑                                   ↑                       ↑
     *  base                                current (marker)        capacity
     *
     * Marker mechanism:
     * - GetMarker() saves current allocation position
     * - FreeToMarker(m) restores to saved position
     * - All allocations after marker are freed
     *
     * Performance characteristics:
     * - Allocation: O(1) - pointer bump (similar to LinearAllocator)
     * - GetMarker: O(1) - instant (just returns offset)
     * - FreeToMarker: O(1) - instant (single pointer write)
     * - Thread-safe: No (use per-thread allocators)
     * - Fragmentation: None (sequential allocation)
     *
     * Limitations:
     * - Must free in LIFO order using markers
     * - Individual Deallocate() is no-op (use markers instead)
     * - Not thread-safe (requires external synchronization)
     * - Fixed capacity (set at construction)
     *
     * Comparison with LinearAllocator:
     * | Feature          | LinearAllocator | StackAllocator |
     * |------------------|-----------------|----------------|
     * | Allocation       | O(1)            | O(1)           |
     * | Individual free  | No              | No             |
     * | Scoped free      | No              | Yes (markers)  |
     * | Reset all        | Yes             | Yes            |
     * | Use case         | Frame temps     | Scoped temps   |
     *
     * Example:
     * @code
     *   comb::StackAllocator stack{1024 * 1024};  // 1 MB
     *
     *   // Outer scope
     *   auto marker1 = stack.GetMarker();
     *   auto* data1 = comb::New<MyData>(stack, args...);
     *
     *   {
     *       // Inner scope
     *       auto marker2 = stack.GetMarker();
     *       auto* temp1 = comb::New<TempData>(stack, args...);
     *       auto* temp2 = comb::New<TempData>(stack, args...);
     *
     *       // Use temp1, temp2...
     *
     *       stack.FreeToMarker(marker2);  // Free temp1, temp2
     *   }
     *
     *   // data1 still valid here
     *
     *   stack.FreeToMarker(marker1);  // Free everything
     * @endcode
     */
    class StackAllocator
    {
    public:
        using Marker = size_t;

        /**
         * Construct stack allocator with given capacity
         * @param capacity Size in bytes to allocate from OS
         */
        explicit StackAllocator(size_t capacity)
            : capacity_{capacity}
            , current_{0}
        {
            hive::Assert(capacity > 0, "Stack capacity must be > 0");

            memory_block_ = AllocatePages(capacity);
            hive::Assert(memory_block_ != nullptr, "Failed to allocate stack memory");

#if COMB_MEM_DEBUG
            registry_ = std::make_unique<debug::AllocationRegistry>();
            history_ = std::make_unique<debug::AllocationHistory>();
            release_current_ = 0;
            debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), registry_.get());
#endif
        }

        ~StackAllocator()
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
                FreePages(memory_block_, capacity_);
            }
        }

        StackAllocator(const StackAllocator&) = delete;
        StackAllocator& operator=(const StackAllocator&) = delete;

        StackAllocator(StackAllocator&& other) noexcept
            : memory_block_{other.memory_block_}
            , capacity_{other.capacity_}
            , current_{other.current_}
#if COMB_MEM_DEBUG
            , registry_{std::move(other.registry_)}
            , history_{std::move(other.history_)}
            , release_current_{other.release_current_}
#endif
        {
            other.memory_block_ = nullptr;
            other.capacity_ = 0;
            other.current_ = 0;
#if COMB_MEM_DEBUG
            other.release_current_ = 0;
#endif
        }

        StackAllocator& operator=(StackAllocator&& other) noexcept
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
                    FreePages(memory_block_, capacity_);
                }

                memory_block_ = other.memory_block_;
                capacity_ = other.capacity_;
                current_ = other.current_;

#if COMB_MEM_DEBUG
                registry_ = std::move(other.registry_);
                history_ = std::move(other.history_);
                release_current_ = other.release_current_;
#endif

                other.memory_block_ = nullptr;
                other.capacity_ = 0;
                other.current_ = 0;
#if COMB_MEM_DEBUG
                other.release_current_ = 0;
#endif
            }
            return *this;
        }

        /**
         * Allocate memory with specified size and alignment
         * Bumps current pointer forward
         *
         * @param size Number of bytes to allocate
         * @param alignment Required alignment (must be power of 2)
         * @param tag Optional allocation tag for debugging (e.g., "TempBuffer")
         * @return Pointer to allocated memory, or nullptr if out of space
         *
         * Note: tag parameter is zero-cost when COMB_MEM_DEBUG=0
         *
         * IMPORTANT: Does NOT support individual deallocation.
         * Use GetMarker() + FreeToMarker() for scoped cleanup.
         */
        [[nodiscard]] void* Allocate(size_t size, size_t alignment, const char* tag = nullptr)
        {
#if COMB_MEM_DEBUG
            return AllocateDebug(size, alignment, tag);
#else
            (void)tag;  // Suppress unused warning

            hive::Assert(size > 0, "Cannot allocate 0 bytes");
            hive::Assert(IsPowerOfTwo(alignment), "Alignment must be power of 2");

            // Calculate the actual current address
            const uintptr_t current_addr = reinterpret_cast<uintptr_t>(memory_block_) + current_;

            // Align the address (not just the offset!)
            const uintptr_t aligned_addr = AlignUp(current_addr, alignment);

            // Calculate the new offset from base
            const size_t aligned_current = aligned_addr - reinterpret_cast<uintptr_t>(memory_block_);
            const size_t padding = aligned_current - current_;

            // Check if we have enough space
            const size_t required = padding + size;
            const size_t remaining = capacity_ - current_;

            if (required > remaining)
            {
                return nullptr;  // Out of memory
            }

            // Allocate
            current_ = aligned_current + size;
            return reinterpret_cast<void*>(aligned_addr);
#endif
        }

        /**
         * Deallocate memory - NO-OP for StackAllocator
         *
         * @param ptr Pointer to deallocate (ignored)
         *
         * NOTE: Individual deallocation is not supported.
         * Use GetMarker() + FreeToMarker() for scoped cleanup,
         * or Reset() to free all memory.
         */
        void Deallocate(void* ptr)
        {
#if COMB_MEM_DEBUG
            DeallocateDebug(ptr);
#else
            (void)ptr;
#endif
        }

        /**
         * Get current marker position
         * Save this to later restore allocator state
         *
         * @return Marker representing current allocation position
         */
        [[nodiscard]] Marker GetMarker() const
        {
            return current_;
        }

        /**
         * Free all allocations back to a saved marker
         * Restores allocator to state when marker was created
         *
         * @param marker Marker obtained from GetMarker()
         *
         * IMPORTANT: Marker must be valid (from this allocator).
         * Passing invalid marker is undefined behavior.
         * Markers must be freed in LIFO order (stack discipline).
         */
        void FreeToMarker(Marker marker)
        {
            hive::Assert(marker <= current_, "Invalid marker (beyond current position)");
            hive::Assert(marker <= capacity_, "Invalid marker (beyond capacity)");

            current_ = marker;

#if COMB_MEM_DEBUG
            if (registry_)
            {
                void* markerAddress = static_cast<std::byte*>(memory_block_) + marker;
                release_current_ = registry_->CalculateBytesUsedUpTo(markerAddress);
                registry_->ClearAllocationsFrom(markerAddress);
            }
#endif
        }

        /**
         * Reset allocator - frees all allocations
         * Equivalent to FreeToMarker(0)
         */
        void Reset()
        {
            current_ = 0;

#if COMB_MEM_DEBUG
            release_current_ = 0;
            if (registry_)
            {
                registry_->Clear();
            }
#endif
        }

        /**
         * Get number of bytes currently allocated
         * @return Bytes used
         */
        [[nodiscard]] size_t GetUsedMemory() const
        {
#if COMB_MEM_DEBUG
            return release_current_;
#else
            return current_;
#endif
        }

        /**
         * Get total capacity of allocator
         * @return Total bytes available
         */
        [[nodiscard]] size_t GetTotalMemory() const
        {
            return capacity_;
        }

        /**
         * Get allocator name for debugging
         * @return "StackAllocator"
         */
        [[nodiscard]] const char* GetName() const
        {
            return "StackAllocator";
        }

        /**
         * Get number of free bytes remaining
         * @return Bytes available for allocation
         */
        [[nodiscard]] size_t GetFreeMemory() const
        {
#if COMB_MEM_DEBUG
            return capacity_ - release_current_;
#else
            return capacity_ - current_;
#endif
        }

    private:
        void* memory_block_{nullptr};   // Base memory block
        size_t capacity_{0};             // Total capacity in bytes
        size_t current_{0};              // Current allocation offset

#if COMB_MEM_DEBUG
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> registry_;
        std::unique_ptr<debug::AllocationHistory> history_;

        // Virtual "release mode" offset - tracks what current_ would be without guard bytes
        size_t release_current_{0};
#endif
    };

    static_assert(Allocator<StackAllocator>, "StackAllocator must satisfy Allocator concept");

#if COMB_MEM_DEBUG
    // ========================================================================
    // Debug Implementation (Inline methods - must be in header)
    // ========================================================================

    inline void* StackAllocator::AllocateDebug(size_t size, size_t alignment, const char* tag)
    {
        using namespace debug;

        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be power of 2");
        hive::Assert(size > 0, "Cannot allocate 0 bytes");

        // 1. Calculate space needed (user size + guard bytes)
        const size_t guardSize = sizeof(uint32_t);
        const size_t totalSize = size + 2 * guardSize;

        // 2. Align the USER pointer (after front guard), not the raw pointer
        //    Layout: [GUARD_FRONT (4B)][user data (aligned)][GUARD_BACK (4B)]
        const uintptr_t current_addr = reinterpret_cast<uintptr_t>(memory_block_) + current_;
        const uintptr_t user_addr_unaligned = current_addr + guardSize;
        const uintptr_t user_addr_aligned = AlignUp(user_addr_unaligned, alignment);
        const uintptr_t raw_addr = user_addr_aligned - guardSize;

        const size_t raw_offset = raw_addr - reinterpret_cast<uintptr_t>(memory_block_);
        const size_t padding = raw_addr - current_addr;
        const size_t required = padding + totalSize;
        const size_t remaining = capacity_ - current_;

        if (required > remaining)
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] [{}] Allocation failed: size={}, alignment={}, tag={}",
                           GetName(), size, alignment, tag ? tag : "<no tag>");
            return nullptr;
        }

        void* rawPtr = reinterpret_cast<void*>(raw_addr);
        current_ = raw_offset + totalSize;

        // Advance virtual release offset as if we were in release mode (no guard bytes)
        const size_t release_aligned = AlignUp(release_current_, alignment);
        release_current_ = release_aligned + size;

        // 3. Write guard bytes
        auto* guardFront = static_cast<uint32_t*>(rawPtr);
        *guardFront = GuardMagic;

        void* userPtr = reinterpret_cast<void*>(user_addr_aligned);

        auto* guardBack = reinterpret_cast<uint32_t*>(
            static_cast<std::byte*>(userPtr) + size
        );
        *guardBack = GuardMagic;

        // 4. Initialize memory with pattern (detect uninitialized reads)
        if constexpr (kMemDebugEnabled)
        {
            std::memset(userPtr, AllocatedMemoryPattern, size);
        }

        // 5. Register allocation
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

        // 6. Record in history
#if COMB_MEM_DEBUG_HISTORY
        history_->RecordAllocation(info);
#endif

        return userPtr;
    }

    inline void StackAllocator::DeallocateDebug(void* ptr)
    {
        using namespace debug;

        // StackAllocator doesn't support individual deallocation
        // But we still track it for debugging purposes

        if (!ptr) return;

        // 1. Find allocation info
        auto* info = registry_->FindAllocation(ptr);
        if (!info)
        {
            hive::LogWarning(comb::LogCombRoot,
                             "[MEM_DEBUG] [{}] Deallocate called on unknown pointer: {}",
                             GetName(), ptr);
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

        // NOTE: StackAllocator doesn't actually free individual allocations
        // Memory is only freed on FreeToMarker() or Reset()
    }

#endif // COMB_MEM_DEBUG
}
