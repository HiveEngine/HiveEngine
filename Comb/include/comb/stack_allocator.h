#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>
#include <comb/platform.h>
#include <comb/utils.h>

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
    // Stack allocator with LIFO deallocation via markers.
    // Like LinearAllocator but supports scoped rollback with GetMarker/FreeToMarker.
    class StackAllocator
    {
    public:
        using Marker = size_t;

        /**
         * Construct stack allocator with given capacity
         * @param capacity Size in bytes to allocate from OS
         */
        explicit StackAllocator(size_t capacity)
            : m_capacity{capacity}
            , m_current{0}
        {
            hive::Assert(capacity > 0, "Stack capacity must be > 0");

            m_memoryBlock = AllocatePages(capacity);
            hive::Assert(m_memoryBlock != nullptr, "Failed to allocate stack memory");

#if COMB_MEM_DEBUG
            m_registry = std::make_unique<debug::AllocationRegistry>();
            m_history = std::make_unique<debug::AllocationHistory>();
            m_releaseCurrent = 0;
            debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), m_registry.get());
#endif
        }

        ~StackAllocator()
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

        StackAllocator(const StackAllocator&) = delete;
        StackAllocator& operator=(const StackAllocator&) = delete;

        StackAllocator(StackAllocator&& other) noexcept
            : m_memoryBlock{other.m_memoryBlock}
            , m_capacity{other.m_capacity}
            , m_current{other.m_current}
#if COMB_MEM_DEBUG
            , m_registry{std::move(other.m_registry)}
            , m_history{std::move(other.m_history)}
            , m_releaseCurrent{other.m_releaseCurrent}
#endif
        {
            other.m_memoryBlock = nullptr;
            other.m_capacity = 0;
            other.m_current = 0;
#if COMB_MEM_DEBUG
            other.m_releaseCurrent = 0;
#endif
        }

        StackAllocator& operator=(StackAllocator&& other) noexcept
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
                m_current = other.m_current;

#if COMB_MEM_DEBUG
                m_registry = std::move(other.m_registry);
                m_history = std::move(other.m_history);
                m_releaseCurrent = other.m_releaseCurrent;
#endif

                other.m_memoryBlock = nullptr;
                other.m_capacity = 0;
                other.m_current = 0;
#if COMB_MEM_DEBUG
                other.m_releaseCurrent = 0;
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
            void* result = AllocateDebug(size, alignment, tag);
#else
            (void)tag; // Suppress unused warning

            hive::Assert(size > 0, "Cannot allocate 0 bytes");
            hive::Assert(IsPowerOfTwo(alignment), "Alignment must be power of 2");

            // Calculate the actual current address
            const uintptr_t current_addr = reinterpret_cast<uintptr_t>(m_memoryBlock) + m_current;

            // Align the address (not just the offset!)
            const uintptr_t aligned_addr = AlignUp(current_addr, alignment);

            // Calculate the new offset from base
            const size_t aligned_current = aligned_addr - reinterpret_cast<uintptr_t>(m_memoryBlock);
            const size_t padding = aligned_current - m_current;

            // Check if we have enough space
            const size_t required = padding + size;
            const size_t remaining = m_capacity - m_current;

            if (required > remaining)
            {
                return nullptr; // Out of memory
            }

            // Allocate
            m_current = aligned_current + size;
            void* result = reinterpret_cast<void*>(aligned_addr);
#endif

            return result;
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
        [[nodiscard]] Marker GetMarker() const noexcept
        {
            return m_current;
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
            hive::Assert(marker <= m_current, "Invalid marker (beyond current position)");
            hive::Assert(marker <= m_capacity, "Invalid marker (beyond capacity)");

            m_current = marker;

#if COMB_MEM_DEBUG
            if (m_registry)
            {
                void* markerAddress = static_cast<std::byte*>(m_memoryBlock) + marker;
                m_releaseCurrent = m_registry->CalculateBytesUsedUpTo(markerAddress);
                m_registry->ClearAllocationsFrom(markerAddress);
            }
#endif
        }

        /**
         * Reset allocator - frees all allocations
         * Equivalent to FreeToMarker(0)
         */
        void Reset()
        {
            m_current = 0;

#if COMB_MEM_DEBUG
            m_releaseCurrent = 0;
            if (m_registry)
            {
                m_registry->Clear();
            }
#endif
        }

        /**
         * Get number of bytes currently allocated
         * @return Bytes used
         */
        [[nodiscard]] size_t GetUsedMemory() const noexcept
        {
#if COMB_MEM_DEBUG
            return m_releaseCurrent;
#else
            return m_current;
#endif
        }

        /**
         * Get total capacity of allocator
         * @return Total bytes available
         */
        [[nodiscard]] size_t GetTotalMemory() const noexcept
        {
            return m_capacity;
        }

        /**
         * Get allocator name for debugging
         * @return "StackAllocator"
         */
        [[nodiscard]] const char* GetName() const noexcept
        {
            return "StackAllocator";
        }

        /**
         * Get number of free bytes remaining
         * @return Bytes available for allocation
         */
        [[nodiscard]] size_t GetFreeMemory() const noexcept
        {
#if COMB_MEM_DEBUG
            return m_capacity - m_releaseCurrent;
#else
            return m_capacity - m_current;
#endif
        }

    private:
        void* m_memoryBlock{nullptr};
        size_t m_capacity{0};
        size_t m_current{0};

#if COMB_MEM_DEBUG
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> m_registry;
        std::unique_ptr<debug::AllocationHistory> m_history;

        // Virtual "release mode" offset - tracks what m_current would be without guard bytes
        size_t m_releaseCurrent{0};
#endif
    };

    static_assert(Allocator<StackAllocator>, "StackAllocator must satisfy Allocator concept");

#if COMB_MEM_DEBUG
    // Debug Implementation (Inline methods - must be in header)

    inline void* StackAllocator::AllocateDebug(size_t size, size_t alignment, const char* tag)
    {
        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be power of 2");
        hive::Assert(size > 0, "Cannot allocate 0 bytes");

        // 1. Calculate space needed (user size + guard bytes)
        const size_t guardSize = sizeof(uint32_t);
        const size_t totalSize = size + 2 * guardSize;

        // 2. Align the USER pointer (after front guard), not the raw pointer
        //    Layout: [GUARD_FRONT (4B)][user data (aligned)][GUARD_BACK (4B)]
        const uintptr_t current_addr = reinterpret_cast<uintptr_t>(m_memoryBlock) + m_current;
        const uintptr_t user_addr_unaligned = current_addr + guardSize;
        const uintptr_t user_addr_aligned = AlignUp(user_addr_unaligned, alignment);
        const uintptr_t raw_addr = user_addr_aligned - guardSize;

        const size_t raw_offset = raw_addr - reinterpret_cast<uintptr_t>(m_memoryBlock);
        const size_t padding = raw_addr - current_addr;
        const size_t required = padding + totalSize;
        const size_t remaining = m_capacity - m_current;

        if (required > remaining)
        {
            hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Allocation failed: size={}, alignment={}, tag={}",
                           GetName(), size, alignment, tag ? tag : "<no tag>");
            return nullptr;
        }

        void* rawPtr = reinterpret_cast<void*>(raw_addr);
        m_current = raw_offset + totalSize;

        // Advance virtual release offset as if we were in release mode (no guard bytes)
        const size_t release_aligned = AlignUp(m_releaseCurrent, alignment);
        m_releaseCurrent = release_aligned + size;

        // 3. Write guard bytes
        debug::WriteGuard(rawPtr);

        void* userPtr = reinterpret_cast<void*>(user_addr_aligned);

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

    inline void StackAllocator::DeallocateDebug(void* ptr)
    {
        // StackAllocator doesn't support individual deallocation
        // But we still track it for debugging purposes

        if (!ptr)
            return;

        // 1. Find allocation info
        auto infoOpt = m_registry->FindAllocation(ptr);
        if (!infoOpt)
        {
            hive::LogWarning(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Deallocate called on unknown pointer: {}",
                             GetName(), ptr);
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
        std::memset(ptr, debug::freedMemoryPattern, info->m_size);
#endif

        // 4. Record deallocation in history
#if COMB_MEM_DEBUG_HISTORY
        m_history->RecordDeallocation(ptr, info->m_size);
#endif

        // 5. Unregister allocation
        m_registry->UnregisterAllocation(ptr);

        // NOTE: StackAllocator doesn't actually free individual allocations
        // Memory is only freed on FreeToMarker() or Reset()
    }

#endif // COMB_MEM_DEBUG
} // namespace comb
