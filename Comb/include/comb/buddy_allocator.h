#pragma once

#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>
#include <comb/platform.h>
#include <comb/utils.h>

#include <array>
#include <cstddef>
#include <memory>
#include <utility>

// Memory debugging (zero overhead when disabled)
#if COMB_MEM_DEBUG
#include <hive/core/log.h>

#include <comb/combmodule.h>
#include <comb/debug/allocation_history.h>
#include <comb/debug/allocation_registry.h>
#include <comb/debug/global_memory_tracker.h>
#include <comb/debug/mem_debug_config.h>
#include <comb/debug/platform_utils.h>

#include <cstring> // For memset
#endif

namespace comb
{
    // Buddy allocator with power-of-2 splitting and coalescing.
    // O(log N) alloc/dealloc, automatic merging of adjacent free blocks.
    // Minimum allocation: 64 bytes. Fixed capacity, not thread-safe.
    class BuddyAllocator
    {
    private:
        static constexpr size_t minBlockSize = 64;
        static constexpr size_t maxLevels = 28; // 64B to 8GB

        struct AllocationHeader
        {
            size_t m_size; // Block size (for deallocation)
        };

        // Header padded to max_align_t so user data starts properly aligned.
        // Blocks are power-of-2 sized and naturally aligned (>= 64B),
        // so block + HeaderPrefix is aligned to max_align_t.
        static constexpr size_t headerPrefix =
            (sizeof(AllocationHeader) + alignof(std::max_align_t) - 1) & ~(alignof(std::max_align_t) - 1);

        // Debug: header + front guard, padded for alignment.
        // Front guard lives in the padding between header and user data.
        static constexpr size_t debugHeaderPrefix =
            (sizeof(AllocationHeader) + sizeof(uint32_t) + alignof(std::max_align_t) - 1) &
            ~(alignof(std::max_align_t) - 1);

        struct FreeBlock
        {
            FreeBlock* m_next;
        };

    public:
        BuddyAllocator(const BuddyAllocator&) = delete;
        BuddyAllocator& operator=(const BuddyAllocator&) = delete;

        /**
         * Construct buddy allocator with specified capacity
         * Capacity will be rounded up to nearest power-of-2
         */
        explicit BuddyAllocator(size_t capacity, const char* debugName = "BuddyAllocator")
            : m_capacity{NextPowerOfTwo(capacity)}
            , m_usedMemory{0}
            , m_debugName{debugName}
        {
            hive::Assert(capacity > 0, "Capacity must be > 0");

            m_memoryBlock = AllocatePages(m_capacity);
            hive::Assert(m_memoryBlock != nullptr, "Failed to allocate buddy memory");

            for (size_t i = 0; i < maxLevels; ++i)
            {
                m_freeLists[i] = nullptr;
            }

            size_t topLevel = GetLevel(m_capacity);
            auto* block = static_cast<FreeBlock*>(m_memoryBlock);
            block->m_next = nullptr;
            m_freeLists[topLevel] = block;

#if COMB_MEM_DEBUG
            m_registry = std::make_unique<debug::AllocationRegistry>();
            m_history = std::make_unique<debug::AllocationHistory>();
            debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), m_registry.get());
#endif
        }

        ~BuddyAllocator()
        {
#if COMB_MEM_DEBUG
            if (m_registry)
            {
                if constexpr (debug::kLeakDetectionEnabled)
                {
                    m_registry->ReportLeaks(GetName());
                }
                debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(m_registry.get());
            }
#endif

            if (m_memoryBlock)
            {
                FreePages(m_memoryBlock, m_capacity);
            }
        }

        BuddyAllocator(BuddyAllocator&& other) noexcept
            : m_memoryBlock{other.m_memoryBlock}
            , m_capacity{other.m_capacity}
            , m_usedMemory{other.m_usedMemory}
            , m_debugName{other.m_debugName}
            , m_freeLists{other.m_freeLists}
#if COMB_MEM_DEBUG
            , m_registry{std::move(other.m_registry)}
            , m_history{std::move(other.m_history)}
#endif
        {
            other.m_memoryBlock = nullptr;
            other.m_capacity = 0;
            other.m_usedMemory = 0;
            for (size_t i = 0; i < maxLevels; ++i)
            {
                other.m_freeLists[i] = nullptr;
            }
        }

        BuddyAllocator& operator=(BuddyAllocator&& other) noexcept
        {
            if (this != &other)
            {
#if COMB_MEM_DEBUG
                if (m_registry)
                {
                    if constexpr (debug::kLeakDetectionEnabled)
                    {
                        m_registry->ReportLeaks(GetName());
                    }
                    debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(m_registry.get());
                }
#endif

                if (m_memoryBlock)
                {
                    FreePages(m_memoryBlock, m_capacity);
                }

                m_memoryBlock = other.m_memoryBlock;
                m_capacity = other.m_capacity;
                m_usedMemory = other.m_usedMemory;
                m_debugName = other.m_debugName;
                m_freeLists = other.m_freeLists;

#if COMB_MEM_DEBUG
                m_registry = std::move(other.m_registry);
                m_history = std::move(other.m_history);
#endif

                other.m_memoryBlock = nullptr;
                other.m_capacity = 0;
                other.m_usedMemory = 0;
                for (size_t i = 0; i < maxLevels; ++i)
                {
                    other.m_freeLists[i] = nullptr;
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
            void* ptr = AllocateDebug(size, alignment, tag);
#else
            (void)tag; // Suppress unused warning

            hive::Assert(alignment <= alignof(std::max_align_t), "BuddyAllocator alignment limited to max_align_t");

            size_t totalSize = size + headerPrefix;
            size_t blockSize = NextPowerOfTwo(totalSize);
            if (blockSize < minBlockSize)
            {
                blockSize = minBlockSize;
            }

            size_t level = GetLevel(blockSize);

            size_t currentLevel = level;
            while (currentLevel < maxLevels && m_freeLists[currentLevel] == nullptr)
            {
                ++currentLevel;
            }

            if (currentLevel >= maxLevels)
            {
                return nullptr;
            }

            FreeBlock* block = m_freeLists[currentLevel];
            m_freeLists[currentLevel] = block->m_next;

            while (currentLevel > level)
            {
                --currentLevel;

                size_t splitSize = GetBlockSize(currentLevel);

                auto* buddy = reinterpret_cast<FreeBlock*>(reinterpret_cast<std::byte*>(block) + splitSize);

                buddy->m_next = m_freeLists[currentLevel];
                m_freeLists[currentLevel] = buddy;
            }

            auto* header = reinterpret_cast<AllocationHeader*>(block);
            header->m_size = blockSize;

            m_usedMemory += blockSize;

            void* ptr = reinterpret_cast<std::byte*>(block) + headerPrefix;
#endif

            if (ptr)
            {
                HIVE_PROFILE_ALLOC(ptr, size, GetName());
            }
            return ptr;
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
            if (!ptr)
                return;

            HIVE_PROFILE_FREE(ptr, GetName());

#if COMB_MEM_DEBUG
            DeallocateDebug(ptr);
#else
            auto* header = reinterpret_cast<AllocationHeader*>(static_cast<std::byte*>(ptr) - headerPrefix);

            size_t blockSize = header->m_size;
            size_t level = GetLevel(blockSize);

            m_usedMemory -= blockSize;

            void* blockPtr = header;
            CoalesceAndInsert(blockPtr, blockSize, level);
#endif
        }

        /**
         * Get total bytes currently allocated
         */
        [[nodiscard]] size_t GetUsedMemory() const noexcept
        {
            return m_usedMemory;
        }

        /**
         * Get total capacity
         */
        [[nodiscard]] size_t GetTotalMemory() const noexcept
        {
            return m_capacity;
        }

        /**
         * Get allocator name for debugging
         */
        [[nodiscard]] const char* GetName() const noexcept
        {
            return m_debugName;
        }

        /**
         * Get the usable size of an allocation's block
         *
         * Returns how many bytes can safely be read/written from the
         * user pointer. This is the buddy block size minus header overhead,
         * which is always >= the originally requested size.
         *
         * @param ptr User pointer previously returned by Allocate
         * @return Usable byte count, or 0 if ptr is null
         */
        [[nodiscard]] size_t GetBlockUsableSize(const void* ptr) const
        {
            if (!ptr)
                return 0;
#if COMB_MEM_DEBUG
            auto* header =
                reinterpret_cast<const AllocationHeader*>(static_cast<const std::byte*>(ptr) - debugHeaderPrefix);
            return header->m_size - debugHeaderPrefix;
#else
            auto* header = reinterpret_cast<const AllocationHeader*>(static_cast<const std::byte*>(ptr) - headerPrefix);
            return header->m_size - headerPrefix;
#endif
        }

        /**
         * Reset allocator to initial state
         *
         * All existing allocations become invalid.
         * Rebuilds the free list as a single top-level block.
         */
        [[nodiscard]] void* GetBaseAddress() const noexcept
        {
            return m_memoryBlock;
        }

        void Reset()
        {
            for (size_t i = 0; i < maxLevels; ++i)
            {
                m_freeLists[i] = nullptr;
            }

            size_t topLevel = GetLevel(m_capacity);
            auto* block = static_cast<FreeBlock*>(m_memoryBlock);
            block->m_next = nullptr;
            m_freeLists[topLevel] = block;

            m_usedMemory = 0;

#if COMB_MEM_DEBUG
            if (m_registry)
            {
                m_registry->Clear();
            }
#endif
        }

    private:
        // Convert size to level (0 = 64B, 1 = 128B, 2 = 256B, etc.)
        [[nodiscard]] constexpr size_t GetLevel(size_t size) const noexcept
        {
            size_t blockSize = minBlockSize;
            size_t level = 0;

            while (blockSize < size && level < maxLevels)
            {
                blockSize <<= 1;
                ++level;
            }

            return level;
        }

        // Convert level to block size
        [[nodiscard]] constexpr size_t GetBlockSize(size_t level) const noexcept
        {
            return minBlockSize << level;
        }

        // Calculate buddy offset using XOR
        [[nodiscard]] size_t GetBuddyOffset(size_t offset, size_t blockSize) const noexcept
        {
            return offset ^ blockSize;
        }

        // Coalesce and insert block into free list
        void CoalesceAndInsert(void* blockPtr, size_t blockSize, size_t level)
        {
            // Calculate offset
            size_t offset =
                static_cast<size_t>(static_cast<std::byte*>(blockPtr) - static_cast<std::byte*>(m_memoryBlock));

            // Try to merge with buddy
            while (level < maxLevels - 1)
            {
                size_t buddyOffset = GetBuddyOffset(offset, blockSize);

                // Check if buddy is in our memory range
                if (buddyOffset >= m_capacity)
                {
                    break;
                }

                void* buddyPtr = static_cast<std::byte*>(m_memoryBlock) + buddyOffset;

                // Search for buddy in free list
                FreeBlock* prev = nullptr;
                FreeBlock* curr = m_freeLists[level];
                bool buddyFound = false;

                while (curr != nullptr)
                {
                    if (curr == buddyPtr)
                    {
                        buddyFound = true;
                        break;
                    }
                    prev = curr;
                    curr = curr->m_next;
                }

                if (!buddyFound)
                {
                    break; // Buddy not free, stop coalescing
                }

                // Remove buddy from free list
                if (prev)
                {
                    prev->m_next = curr->m_next;
                }
                else
                {
                    m_freeLists[level] = curr->m_next;
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
            block->m_next = m_freeLists[level];
            m_freeLists[level] = block;
        }

        void* m_memoryBlock{nullptr};
        size_t m_capacity{0};
        size_t m_usedMemory{0};
        const char* m_debugName{"BuddyAllocator"};
        std::array<FreeBlock*, maxLevels> m_freeLists{};

#if COMB_MEM_DEBUG
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> m_registry;
        std::unique_ptr<debug::AllocationHistory> m_history;
#endif
    };

    static_assert(Allocator<BuddyAllocator>, "BuddyAllocator must satisfy Allocator concept");

#if COMB_MEM_DEBUG
    // Debug Implementation (Inline methods - must be in header)

    inline void* BuddyAllocator::AllocateDebug(size_t size, size_t alignment, const char* tag)
    {
        hive::Assert(alignment <= alignof(std::max_align_t), "BuddyAllocator alignment limited to max_align_t");

        // 1. Calculate space needed
        // Layout: [AllocationHeader][...padding...][GUARD_FRONT (4B)][user data][GUARD_BACK (4B)]
        // User data at block + DebugHeaderPrefix, properly aligned.
        // Front guard fits in the padding (guaranteed by DebugHeaderPrefix formula).
        const size_t guardSize = sizeof(uint32_t);
        size_t totalSize = debugHeaderPrefix + size + guardSize;
        size_t blockSize = NextPowerOfTwo(totalSize);
        if (blockSize < minBlockSize)
        {
            blockSize = minBlockSize;
        }

        size_t level = GetLevel(blockSize);

        // 2. Find a free block (release path)
        size_t currentLevel = level;
        while (currentLevel < maxLevels && m_freeLists[currentLevel] == nullptr)
        {
            ++currentLevel;
        }

        if (currentLevel >= maxLevels)
        {
            hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Allocation failed: size={}, alignment={}, tag={}",
                           GetName(), size, alignment, tag ? tag : "<no tag>");
            return nullptr; // No block available
        }

        // Pop block from free list
        FreeBlock* block = m_freeLists[currentLevel];
        m_freeLists[currentLevel] = block->m_next;

        // Split down to desired level
        while (currentLevel > level)
        {
            --currentLevel;

            size_t splitSize = GetBlockSize(currentLevel);

            // Split: second half becomes buddy
            auto* buddy = reinterpret_cast<FreeBlock*>(reinterpret_cast<std::byte*>(block) + splitSize);

            // Add buddy to free list
            buddy->m_next = m_freeLists[currentLevel];
            m_freeLists[currentLevel] = buddy;
        }

        // Write header
        auto* header = reinterpret_cast<AllocationHeader*>(block);
        header->m_size = blockSize;

        // Track used_memory_ with release layout (no guards) for consistent stats
        size_t releaseTotalSize = size + headerPrefix;
        size_t releaseBlockSize = NextPowerOfTwo(releaseTotalSize);
        if (releaseBlockSize < minBlockSize)
        {
            releaseBlockSize = minBlockSize;
        }
        m_usedMemory += releaseBlockSize;

        // 3. Place guard bytes and user data
        void* userPtr = reinterpret_cast<std::byte*>(block) + debugHeaderPrefix;

        debug::WriteGuard(static_cast<std::byte*>(userPtr) - guardSize);
        debug::WriteGuard(static_cast<std::byte*>(userPtr) + size);

        // 4. Initialize memory with pattern (detect uninitialized reads)
        if constexpr (debug::kMemDebugEnabled)
        {
            std::memset(userPtr, debug::allocatedMemoryPattern, size);
        }

        // 5. Register allocation
        debug::AllocationInfo info{};
        info.m_address = userPtr;
        info.m_size = size;
        info.m_alignment = alignment;
        info.m_timestamp = debug::GetTimestamp();
        info.m_tag = tag;
        info.m_allocationId = m_registry->GetNextAllocationId();
        info.m_threadId = debug::GetThreadId();

#if COMB_MEM_DEBUG_CALLSTACKS
        debug::CaptureCallstack(info.callstack, info.callstackDepth);
#endif

        m_registry->RegisterAllocation(info);

        // 6. Record in history
#if COMB_MEM_DEBUG_HISTORY
        m_history->RecordAllocation(info);
#endif

        return userPtr;
    }

    inline void BuddyAllocator::DeallocateDebug(void* ptr)
    {
        if (!ptr)
            return;

        // 1. Find allocation info
        auto infoOpt = m_registry->FindAllocation(ptr);
        if (!infoOpt)
        {
            hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Double-free or invalid pointer detected! Address: {}",
                           GetName(), ptr);
            hive::Assert(false, "Double-free or invalid pointer (not found in registry)");
            return;
        }
        auto& info = *infoOpt;

        // 2. Check guard bytes
        if constexpr (debug::kMemDebugEnabled)
        {
            if (!info.CheckGuards())
            {
                if (info.ReadGuardFront() != debug::guardMagic)
                {
                    hive::LogError(comb::LOG_COMB_ROOT,
                                   "[MEM_DEBUG] [{}] Buffer UNDERRUN detected! Address: {}, Size: {}, Tag: {}",
                                   GetName(), ptr, info.m_size, info.GetTagOrDefault());
                    hive::Assert(false, "Buffer underrun detected");
                }

                if (info.ReadGuardBack() != debug::guardMagic)
                {
                    hive::LogError(comb::LOG_COMB_ROOT,
                                   "[MEM_DEBUG] [{}] Buffer OVERRUN detected! Address: {}, Size: {}, Tag: {}",
                                   GetName(), ptr, info.m_size, info.GetTagOrDefault());
                    hive::Assert(false, "Buffer overrun detected");
                }
            }
        }

        // 3. Calculate release block size BEFORE unregistering (info will be destroyed)
        // Track memory using release mode calculation (excluding guard bytes)
        // This ensures GetUsedMemory() returns consistent values between debug and release
        size_t releaseTotalSize = info->m_size + headerPrefix;
        size_t releaseBlockSize = NextPowerOfTwo(releaseTotalSize);
        if (releaseBlockSize < minBlockSize)
        {
            releaseBlockSize = minBlockSize;
        }

        // 4. Fill with freed pattern (detect use-after-free)
#if COMB_MEM_DEBUG_USE_AFTER_FREE
        std::memset(ptr, debug::freedMemoryPattern, info->m_size);
#endif

        // 5. Record deallocation in history
#if COMB_MEM_DEBUG_HISTORY
        m_history->RecordDeallocation(ptr, info->m_size);
#endif

        // 6. Unregister allocation (info pointer becomes invalid after this!)
        m_registry->UnregisterAllocation(ptr);

        // 7. Get block pointer (before header)
        // Layout: [AllocationHeader][...padding...][Guard Front][user data][Guard Back]
        // ptr = userPtr at block + DebugHeaderPrefix
        void* blockPtr = static_cast<std::byte*>(ptr) - debugHeaderPrefix;

        auto* header = reinterpret_cast<AllocationHeader*>(blockPtr);
        size_t blockSize = header->m_size;
        size_t level = GetLevel(blockSize);

        // Decrement used memory
        m_usedMemory -= releaseBlockSize;

        // 8. Start coalescing
        CoalesceAndInsert(blockPtr, blockSize, level);
    }

#endif // COMB_MEM_DEBUG
} // namespace comb
