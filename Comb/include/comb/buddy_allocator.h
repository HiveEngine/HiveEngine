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
     * Buddy allocator with power-of-2 splitting and coalescing
     *
     * Manages memory using binary buddy system:
     * - All allocations rounded to power-of-2
     * - Large blocks split into smaller buddies
     * - Adjacent free buddies automatically merged
     * - Minimal fragmentation through coalescing
     *
     * Satisfies the comb::Allocator concept.
     *
     * Use cases:
     * - General-purpose allocation with mixed sizes
     * - Long-lived allocations (assets, subsystems)
     * - Need for deallocation with minimal fragmentation
     * - Alternative to malloc with better performance
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────┐
     * │ Level 0: 64B    64B    64B    64B    ...               │
     * │ Level 1: 128B        128B        128B        ...       │
     * │ Level 2: 256B             256B             ...         │
     * │ Level 3: 512B                      512B      ...       │
     * └────────────────────────────────────────────────────────┘
     *
     * Each level maintains a free-list of blocks of that size.
     * When allocating, find smallest suitable block and split if needed.
     * When deallocating, merge with buddy if both are free.
     *
     * Buddy calculation:
     *   buddy_offset = offset XOR block_size
     *
     * Performance characteristics:
     * - Allocation: O(log N) - find/split blocks
     * - Deallocation: O(log N) - merge buddies
     * - Coalescing: Automatic (happens during deallocation)
     * - Thread-safe: No (use per-thread or add synchronization)
     * - Fragmentation: Low (internal only, power-of-2 rounding)
     *
     * Limitations:
     * - Fixed capacity (set at construction)
     * - Internal fragmentation (round to power-of-2)
     * - Returns nullptr when out of memory (no hidden allocations)
     * - Not thread-safe by default
     * - Minimum allocation size: 64 bytes
     *
     * Example:
     * @code
     *   // Create buddy allocator with 1 MB capacity
     *   comb::BuddyAllocator buddy{1 * 1024 * 1024};
     *
     *   // Allocate 100 bytes - rounded to 128 bytes
     *   void* ptr1 = buddy.Allocate(100, 8);
     *   if (!ptr1) {
     *       // Out of memory
     *       return;
     *   }
     *
     *   // Allocate 200 bytes - rounded to 256 bytes
     *   void* ptr2 = buddy.Allocate(200, 8);
     *
     *   // Free memory - automatically merges with buddy if free
     *   buddy.Deallocate(ptr1);
     *   buddy.Deallocate(ptr2);
     *
     *   // Memory coalesced, available for reuse
     * @endcode
     */
    class BuddyAllocator
    {
    private:
        static constexpr size_t MinBlockSize = 64;
        static constexpr size_t MaxLevels = 28;  // 64B to 8GB

        struct AllocationHeader
        {
            size_t size;  // Block size (for deallocation)
        };

        struct FreeBlock
        {
            FreeBlock* next;
        };

    public:
        BuddyAllocator(const BuddyAllocator&) = delete;
        BuddyAllocator& operator=(const BuddyAllocator&) = delete;

        /**
         * Construct buddy allocator with specified capacity
         * Capacity will be rounded up to nearest power-of-2
         */
        explicit BuddyAllocator(size_t capacity)
            : capacity_{NextPowerOfTwo(capacity)}
            , used_memory_{0}
        {
            hive::Assert(capacity > 0, "Capacity must be > 0");

            memory_block_ = AllocatePages(capacity_);
            hive::Assert(memory_block_ != nullptr, "Failed to allocate buddy memory");

            for (size_t i = 0; i < MaxLevels; ++i)
            {
                free_lists_[i] = nullptr;
            }

            size_t topLevel = GetLevel(capacity_);
            auto* block = static_cast<FreeBlock*>(memory_block_);
            block->next = nullptr;
            free_lists_[topLevel] = block;

#if COMB_MEM_DEBUG
            registry_ = std::make_unique<debug::AllocationRegistry>();
            history_ = std::make_unique<debug::AllocationHistory>();
            debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), registry_.get());
#endif
        }

        ~BuddyAllocator()
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

        BuddyAllocator(BuddyAllocator&& other) noexcept
            : memory_block_{other.memory_block_}
            , capacity_{other.capacity_}
            , used_memory_{other.used_memory_}
            , free_lists_{other.free_lists_}
#if COMB_MEM_DEBUG
            , registry_{std::move(other.registry_)}
            , history_{std::move(other.history_)}
#endif
        {
            other.memory_block_ = nullptr;
            other.capacity_ = 0;
            other.used_memory_ = 0;
            for (size_t i = 0; i < MaxLevels; ++i)
            {
                other.free_lists_[i] = nullptr;
            }
        }

        BuddyAllocator& operator=(BuddyAllocator&& other) noexcept
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
                used_memory_ = other.used_memory_;
                free_lists_ = other.free_lists_;

#if COMB_MEM_DEBUG
                registry_ = std::move(other.registry_);
                history_ = std::move(other.history_);
#endif

                other.memory_block_ = nullptr;
                other.capacity_ = 0;
                other.used_memory_ = 0;
                for (size_t i = 0; i < MaxLevels; ++i)
                {
                    other.free_lists_[i] = nullptr;
                }
            }
            return *this;
        }

        /**
         * Allocate memory using buddy system
         *
         * Size is rounded up to next power-of-2.
         * Finds smallest suitable block, splitting if needed.
         *
         * @param size Number of bytes to allocate
         * @param alignment Required alignment (must be <= alignof(std::max_align_t))
         * @param tag Optional allocation tag for debugging (e.g., "AssetLoader")
         * @return Pointer to allocated memory, or nullptr if:
         *         - No block large enough available
         *         - Out of memory
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
                         "BuddyAllocator alignment limited to max_align_t");

            size_t totalSize = size + sizeof(AllocationHeader);
            size_t blockSize = NextPowerOfTwo(totalSize);
            if (blockSize < MinBlockSize)
            {
                blockSize = MinBlockSize;
            }

            size_t level = GetLevel(blockSize);

            size_t currentLevel = level;
            while (currentLevel < MaxLevels && free_lists_[currentLevel] == nullptr)
            {
                ++currentLevel;
            }

            if (currentLevel >= MaxLevels)
            {
                return nullptr;
            }

            FreeBlock* block = free_lists_[currentLevel];
            free_lists_[currentLevel] = block->next;

            while (currentLevel > level)
            {
                --currentLevel;

                size_t splitSize = GetBlockSize(currentLevel);

                auto* buddy = reinterpret_cast<FreeBlock*>(
                    reinterpret_cast<std::byte*>(block) + splitSize
                );

                buddy->next = free_lists_[currentLevel];
                free_lists_[currentLevel] = buddy;
            }

            auto* header = reinterpret_cast<AllocationHeader*>(block);
            header->size = blockSize;

            used_memory_ += blockSize;

            void* ptr = reinterpret_cast<std::byte*>(block) + sizeof(AllocationHeader);
            return ptr;
#endif
        }

        /**
         * Deallocate memory back to buddy system
         *
         * Automatically merges with buddy if both are free.
         *
         * @param ptr Pointer to deallocate (can be nullptr)
         *
         * IMPORTANT: Pointer must have been allocated from THIS allocator.
         */
        void Deallocate(void* ptr)
        {
#if COMB_MEM_DEBUG
            DeallocateDebug(ptr);
#else
            if (!ptr)
                return;

            auto* header = reinterpret_cast<AllocationHeader*>(
                static_cast<std::byte*>(ptr) - sizeof(AllocationHeader)
            );

            size_t blockSize = header->size;
            size_t level = GetLevel(blockSize);

            used_memory_ -= blockSize;

            void* blockPtr = header;
            CoalesceAndInsert(blockPtr, blockSize, level);
#endif
        }

        /**
         * Get total bytes currently allocated
         */
        [[nodiscard]] size_t GetUsedMemory() const
        {
            return used_memory_;
        }

        /**
         * Get total capacity
         */
        [[nodiscard]] size_t GetTotalMemory() const
        {
            return capacity_;
        }

        /**
         * Get allocator name for debugging
         */
        [[nodiscard]] const char* GetName() const
        {
            return "BuddyAllocator";
        }

    private:
        // Convert size to level (0 = 64B, 1 = 128B, 2 = 256B, etc.)
        constexpr size_t GetLevel(size_t size) const
        {
            size_t blockSize = MinBlockSize;
            size_t level = 0;

            while (blockSize < size && level < MaxLevels)
            {
                blockSize <<= 1;
                ++level;
            }

            return level;
        }

        // Convert level to block size
        constexpr size_t GetBlockSize(size_t level) const
        {
            return MinBlockSize << level;
        }

        // Calculate buddy offset using XOR
        size_t GetBuddyOffset(size_t offset, size_t blockSize) const
        {
            return offset ^ blockSize;
        }

        // Coalesce and insert block into free list
        void CoalesceAndInsert(void* blockPtr, size_t blockSize, size_t level)
        {
            // Calculate offset
            size_t offset = static_cast<size_t>(static_cast<std::byte*>(blockPtr) -
                           static_cast<std::byte*>(memory_block_));

            // Try to merge with buddy
            while (level < MaxLevels - 1)
            {
                size_t buddyOffset = GetBuddyOffset(offset, blockSize);

                // Check if buddy is in our memory range
                if (buddyOffset >= capacity_)
                {
                    break;
                }

                void* buddyPtr = static_cast<std::byte*>(memory_block_) + buddyOffset;

                // Search for buddy in free list
                FreeBlock* prev = nullptr;
                FreeBlock* curr = free_lists_[level];
                bool buddyFound = false;

                while (curr != nullptr)
                {
                    if (curr == buddyPtr)
                    {
                        buddyFound = true;
                        break;
                    }
                    prev = curr;
                    curr = curr->next;
                }

                if (!buddyFound)
                {
                    break;  // Buddy not free, stop coalescing
                }

                // Remove buddy from free list
                if (prev)
                {
                    prev->next = curr->next;
                }
                else
                {
                    free_lists_[level] = curr->next;
                }

                // Merge: parent is at lower offset
                if (offset > buddyOffset)
                {
                    blockPtr = buddyPtr;
                    offset = buddyOffset;
                }

                // Move up one level
                blockSize <<= 1;
                ++level;
            }

            // Insert merged block into free list
            auto* block = static_cast<FreeBlock*>(blockPtr);
            block->next = free_lists_[level];
            free_lists_[level] = block;
        }

        void* memory_block_{nullptr};
        size_t capacity_{0};
        size_t used_memory_{0};
        std::array<FreeBlock*, MaxLevels> free_lists_{};

#if COMB_MEM_DEBUG
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> registry_;
        std::unique_ptr<debug::AllocationHistory> history_;
#endif
    };

    static_assert(Allocator<BuddyAllocator>, "BuddyAllocator must satisfy Allocator concept");

#if COMB_MEM_DEBUG
    // ========================================================================
    // Debug Implementation (Inline methods - must be in header)
    // ========================================================================

    inline void* BuddyAllocator::AllocateDebug(size_t size, size_t alignment, const char* tag)
    {
        using namespace debug;

        hive::Assert(alignment <= alignof(std::max_align_t),
                     "BuddyAllocator alignment limited to max_align_t");

        // 1. Calculate space needed (user size + guard bytes)
        const size_t guardSize = sizeof(uint32_t);
        const size_t debugTotalSize = size + 2 * guardSize;

        // Round size up to power-of-2, including header AND guard bytes
        size_t totalSize = debugTotalSize + sizeof(AllocationHeader);
        size_t blockSize = NextPowerOfTwo(totalSize);
        if (blockSize < MinBlockSize)
        {
            blockSize = MinBlockSize;
        }

        size_t level = GetLevel(blockSize);

        // 2. Find a free block (release path)
        size_t currentLevel = level;
        while (currentLevel < MaxLevels && free_lists_[currentLevel] == nullptr)
        {
            ++currentLevel;
        }

        if (currentLevel >= MaxLevels)
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] [{}] Allocation failed: size={}, alignment={}, tag={}",
                           GetName(), size, alignment, tag ? tag : "<no tag>");
            return nullptr;  // No block available
        }

        // Pop block from free list
        FreeBlock* block = free_lists_[currentLevel];
        free_lists_[currentLevel] = block->next;

        // Split down to desired level
        while (currentLevel > level)
        {
            --currentLevel;

            size_t splitSize = GetBlockSize(currentLevel);

            // Split: second half becomes buddy
            auto* buddy = reinterpret_cast<FreeBlock*>(
                reinterpret_cast<std::byte*>(block) + splitSize
            );

            // Add buddy to free list
            buddy->next = free_lists_[currentLevel];
            free_lists_[currentLevel] = buddy;
        }

        // Write header
        auto* header = reinterpret_cast<AllocationHeader*>(block);
        header->size = blockSize;

        // Track memory using release mode calculation (excluding guard bytes)
        // This ensures GetUsedMemory() returns consistent values between debug and release
        size_t releaseTotalSize = size + sizeof(AllocationHeader);
        size_t releaseBlockSize = NextPowerOfTwo(releaseTotalSize);
        if (releaseBlockSize < MinBlockSize)
        {
            releaseBlockSize = MinBlockSize;
        }
        used_memory_ += releaseBlockSize;

        // 3. Add guard bytes after header
        // Layout: [AllocationHeader][GUARD_FRONT][user data][GUARD_BACK]
        void* rawPtr = reinterpret_cast<std::byte*>(block) + sizeof(AllocationHeader);

        auto* guardFront = static_cast<uint32_t*>(rawPtr);
        *guardFront = GuardMagic;

        void* userPtr = static_cast<std::byte*>(rawPtr) + guardSize;

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

    inline void BuddyAllocator::DeallocateDebug(void* ptr)
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

        // 3. Calculate release block size BEFORE unregistering (info will be destroyed)
        // Track memory using release mode calculation (excluding guard bytes)
        // This ensures GetUsedMemory() returns consistent values between debug and release
        size_t releaseTotalSize = info->size + sizeof(AllocationHeader);
        size_t releaseBlockSize = NextPowerOfTwo(releaseTotalSize);
        if (releaseBlockSize < MinBlockSize)
        {
            releaseBlockSize = MinBlockSize;
        }

        // 4. Fill with freed pattern (detect use-after-free)
#if COMB_MEM_DEBUG_USE_AFTER_FREE
        std::memset(ptr, FreedMemoryPattern, info->size);
#endif

        // 5. Record deallocation in history
#if COMB_MEM_DEBUG_HISTORY
        history_->RecordDeallocation(ptr, info->size);
#endif

        // 6. Unregister allocation (info pointer becomes invalid after this!)
        registry_->UnregisterAllocation(ptr);

        // 7. Get block pointer (before header)
        // Layout: [AllocationHeader][Guard Front][user data][Guard Back]
        // ptr = userPtr
        // We need to go back to the start of the block (before AllocationHeader)
        const size_t guardSize = sizeof(uint32_t);
        void* blockPtr = static_cast<std::byte*>(ptr) - guardSize - sizeof(AllocationHeader);

        auto* header = reinterpret_cast<AllocationHeader*>(blockPtr);
        size_t blockSize = header->size;
        size_t level = GetLevel(blockSize);

        // Decrement used memory
        used_memory_ -= releaseBlockSize;

        // 8. Start coalescing
        CoalesceAndInsert(blockPtr, blockSize, level);
    }

#endif // COMB_MEM_DEBUG
}
