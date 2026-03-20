#pragma once

#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>
#include <comb/platform.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
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
#endif

namespace comb
{
    // Pool allocator for fixed-size objects with free-list recycling.
    // O(1) alloc/dealloc, zero fragmentation. One pool per type T.
    template <typename T> class PoolAllocator
    {
    public:
        /**
         * Construct pool allocator with capacity for N objects
         * @param capacity Number of objects to pre-allocate
         */
        explicit PoolAllocator(size_t capacity)
            : m_capacity{capacity}
            , m_usedCount{0}
        {
            hive::Assert(capacity > 0, "Pool capacity must be > 0");

            constexpr size_t base_slot_size = (sizeof(T) > sizeof(void*)) ? sizeof(T) : sizeof(void*);

#if COMB_MEM_DEBUG
            // Front guard offset padded for proper user data alignment
            constexpr size_t user_align = (alignof(T) > alignof(void*)) ? alignof(T) : alignof(void*);
            constexpr size_t aligned_guard_front = (debug::guardSize + user_align - 1) & ~(user_align - 1);
            // Slot: [front padding][user data][back guard], rounded for next slot alignment
            constexpr size_t slot_size_raw = aligned_guard_front + base_slot_size + debug::guardSize;
            constexpr size_t slot_size = (slot_size_raw + user_align - 1) & ~(user_align - 1);
#else
            constexpr size_t slot_size = base_slot_size;
#endif

            m_totalSize = capacity * slot_size;
            m_memoryBlock = AllocatePages(m_totalSize);
            hive::Assert(m_memoryBlock != nullptr, "Failed to allocate pool memory");

            Reset();

#if COMB_MEM_DEBUG
            m_registry = std::make_unique<debug::AllocationRegistry>();
            m_history = std::make_unique<debug::AllocationHistory>();
            debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), m_registry.get());
#endif
        }

        ~PoolAllocator()
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
                FreePages(m_memoryBlock, m_totalSize);
            }
        }

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        PoolAllocator(PoolAllocator&& other) noexcept
            : m_memoryBlock{other.m_memoryBlock}
            , m_freeListHead{other.m_freeListHead}
            , m_capacity{other.m_capacity}
            , m_usedCount{other.m_usedCount}
            , m_totalSize{other.m_totalSize}
#if COMB_MEM_DEBUG
            , m_registry{std::move(other.m_registry)}
            , m_history{std::move(other.m_history)}
#endif
        {
            other.m_memoryBlock = nullptr;
            other.m_freeListHead = nullptr;
            other.m_capacity = 0;
            other.m_usedCount = 0;
            other.m_totalSize = 0;
        }

        PoolAllocator& operator=(PoolAllocator&& other) noexcept
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
                    FreePages(m_memoryBlock, m_totalSize);
                }

                m_memoryBlock = other.m_memoryBlock;
                m_freeListHead = other.m_freeListHead;
                m_capacity = other.m_capacity;
                m_usedCount = other.m_usedCount;
                m_totalSize = other.m_totalSize;

#if COMB_MEM_DEBUG
                m_registry = std::move(other.m_registry);
                m_history = std::move(other.m_history);
#endif

                other.m_memoryBlock = nullptr;
                other.m_freeListHead = nullptr;
                other.m_capacity = 0;
                other.m_usedCount = 0;
                other.m_totalSize = 0;
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
            (void)tag; // Suppress unused warning

            hive::Assert(size <= sizeof(T), "PoolAllocator can only allocate sizeof(T) bytes");
            hive::Assert(alignment <= alignof(T), "PoolAllocator alignment limited to alignof(T)");

            if (!m_freeListHead)
            {
                return nullptr;
            }

            void* ptr = m_freeListHead;
            m_freeListHead = *static_cast<void**>(m_freeListHead);
            ++m_usedCount;
#endif

            if (ptr)
            {
                HIVE_PROFILE_ALLOC(ptr, size, GetName());
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

            HIVE_PROFILE_FREE(ptr, GetName());

#if COMB_MEM_DEBUG
            DeallocateDebug(ptr);
#else
            hive::Assert(m_usedCount > 0, "Deallocate called more times than Allocate");

            *static_cast<void**>(ptr) = m_freeListHead;
            m_freeListHead = ptr;
            --m_usedCount;
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
            constexpr size_t aligned_guard_front = (debug::guardSize + user_align - 1) & ~(user_align - 1);
            constexpr size_t slot_size_raw = aligned_guard_front + base_slot_size + debug::guardSize;
            constexpr size_t slot_size = (slot_size_raw + user_align - 1) & ~(user_align - 1);
            constexpr size_t guard_offset = aligned_guard_front;
#else
            constexpr size_t slot_size = base_slot_size;
            constexpr size_t guard_offset = 0;
#endif

            auto* current = static_cast<std::byte*>(m_memoryBlock) + guard_offset;
            m_freeListHead = current;

            for (size_t i = 0; i < m_capacity - 1; ++i)
            {
                auto* next = current + slot_size;
                *reinterpret_cast<void**>(current) = next;
                current = next;
            }

            *reinterpret_cast<void**>(current) = nullptr;

            m_usedCount = 0;

#if COMB_MEM_DEBUG
            if (m_registry)
            {
                m_registry->Clear();
            }
#endif
        }

        /**
         * Get number of objects currently allocated
         * @return Number of objects in use
         */
        [[nodiscard]] size_t GetUsedMemory() const noexcept
        {
            return m_usedCount * sizeof(T);
        }

        /**
         * Get total capacity of pool
         * @return Total bytes for all objects
         */
        [[nodiscard]] size_t GetTotalMemory() const noexcept
        {
            return m_capacity * sizeof(T);
        }

        /**
         * Get allocator name for debugging
         * @return "PoolAllocator"
         */
        [[nodiscard]] const char* GetName() const noexcept
        {
            return "PoolAllocator";
        }

        /**
         * Get pool capacity
         * @return Maximum number of objects
         */
        [[nodiscard]] size_t GetCapacity() const noexcept
        {
            return m_capacity;
        }

        /**
         * Get number of objects currently in use
         * @return Number of allocated objects
         */
        [[nodiscard]] size_t GetUsedCount() const noexcept
        {
            return m_usedCount;
        }

        /**
         * Get number of free slots available
         * @return Number of objects that can still be allocated
         */
        [[nodiscard]] size_t GetFreeCount() const noexcept
        {
            return m_capacity - m_usedCount;
        }

    private:
        void* m_memoryBlock{nullptr};
        void* m_freeListHead{nullptr};
        size_t m_capacity{0};
        size_t m_usedCount{0};
        size_t m_totalSize{0};

#if COMB_MEM_DEBUG
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> m_registry;
        std::unique_ptr<debug::AllocationHistory> m_history;
#endif
    };

    template <typename T>
    concept ValidPoolAllocator = Allocator<PoolAllocator<T>>;

#if COMB_MEM_DEBUG
    // Debug Implementation (Template methods - must be in header)

    template <typename T> void* PoolAllocator<T>::AllocateDebug(size_t size, size_t alignment, const char* tag)
    {
        hive::Assert(size <= sizeof(T), "PoolAllocator can only allocate sizeof(T) bytes");
        hive::Assert(alignment <= alignof(T), "PoolAllocator alignment limited to alignof(T)");

        if (!m_freeListHead)
        {
            hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Pool exhausted: size={}, capacity={}, tag={}",
                           GetName(), sizeof(T), m_capacity, tag ? tag : "<no tag>");
            return nullptr;
        }

        void* userPtr = m_freeListHead;
        m_freeListHead = *static_cast<void**>(userPtr);
        ++m_usedCount;

        // 2. Add guard bytes (in-place within the pool slot)
        // Layout: [GUARD_FRONT (4B)][user data (sizeof(T))][GUARD_BACK (4B)]
        // Note: Pool slots are already sized for this in debug builds

        const size_t guardSize = sizeof(uint32_t);
        void* rawPtr = static_cast<std::byte*>(userPtr) - guardSize;

        debug::WriteGuard(rawPtr);
        debug::WriteGuard(static_cast<std::byte*>(userPtr) + sizeof(T));

        // 3. Initialize memory with pattern (detect uninitialized reads)
        if constexpr (debug::kMemDebugEnabled)
        {
            std::memset(userPtr, debug::allocatedMemoryPattern, sizeof(T));
        }

        // 4. Register allocation
        debug::AllocationInfo info{};
        info.m_address = userPtr;
        info.m_size = sizeof(T);
        info.m_alignment = alignof(T);
        info.m_timestamp = debug::GetTimestamp();
        info.m_tag = tag;
        info.m_allocationId = m_registry->GetNextAllocationId();
        info.m_threadId = debug::GetThreadId();

#if COMB_MEM_DEBUG_CALLSTACKS
        debug::CaptureCallstack(info.callstack, info.callstackDepth);
#endif

        m_registry->RegisterAllocation(info);

        // 5. Record in history
#if COMB_MEM_DEBUG_HISTORY
        m_history->RecordAllocation(info);
#endif

        return userPtr;
    }

    template <typename T> void PoolAllocator<T>::DeallocateDebug(void* ptr)
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

        // 3. Fill with freed pattern (detect use-after-free)
#if COMB_MEM_DEBUG_USE_AFTER_FREE
        std::memset(ptr, debug::freedMemoryPattern, sizeof(T));
#endif

        // 4. Record deallocation in history
#if COMB_MEM_DEBUG_HISTORY
        m_history->RecordDeallocation(ptr, sizeof(T));
#endif

        // 5. Unregister allocation
        m_registry->UnregisterAllocation(ptr);

        // 6. Return to free-list
        // In debug mode, free-list stores pointers in user data area (not in guards)
        hive::Assert(m_usedCount > 0, "Deallocate called more times than Allocate");

        // Push to free-list head (ptr is already userPtr in debug mode)
        *static_cast<void**>(ptr) = m_freeListHead;
        m_freeListHead = ptr;
        --m_usedCount;
    }

#endif // COMB_MEM_DEBUG
} // namespace comb
