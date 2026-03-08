#include <hive/core/assert.h>
#include <hive/core/log.h>

#include <comb/combmodule.h>
#include <comb/linear_allocator.h>
#include <comb/platform.h>
#include <comb/precomp.h>
#include <comb/utils.h>

#include <cstring> // For memset

namespace comb
{
    LinearAllocator::LinearAllocator(size_t capacity)
        : m_base{AllocatePages(capacity)}
        , m_current{m_base}
        , m_capacity{capacity}
    {
        hive::Assert(m_base != nullptr, "Failed to allocate memory for LinearAllocator");

#if COMB_MEM_DEBUG
        // Create debug tracking objects
        m_registry = std::make_unique<debug::AllocationRegistry>();
        m_history = std::make_unique<debug::AllocationHistory>();
        m_releaseCurrent = m_base; // Initialize virtual release pointer

        // Register with global tracker
        debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), m_registry.get(), false);
#endif
    }

    LinearAllocator::~LinearAllocator()
    {
#if COMB_MEM_DEBUG
        if (m_registry)
        {
            // NOTE: LinearAllocator doesn't support individual deallocation
            // Having "leaks" at destruction is NORMAL and EXPECTED behavior
            // Users should call Reset() if they want to verify no leaks, but it's not required
            // We DON'T report leaks here because it would be too noisy for normal usage

            // Unregister from global tracker
            debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(m_registry.get());
        }
#endif

        if (m_base)
        {
            FreePages(m_base, m_capacity);
            m_base = nullptr;
            m_current = nullptr;
        }
    }

    LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
        : m_base{other.m_base}
        , m_current{other.m_current}
        , m_capacity{other.m_capacity}
#if COMB_MEM_DEBUG
        , m_registry{std::move(other.m_registry)}
        , m_history{std::move(other.m_history)}
        , m_releaseCurrent{other.m_releaseCurrent}
#endif
    {
        // Note: m_registry and m_history are automatically moved via unique_ptr
        // No need to update global tracker - it still points to the same registry object

        other.m_base = nullptr;
        other.m_current = nullptr;
        other.m_capacity = 0;
#if COMB_MEM_DEBUG
        other.m_releaseCurrent = nullptr;
#endif
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept
    {
        if (this != &other)
        {
#if COMB_MEM_DEBUG
            if (m_registry)
            {
                // Report leaks for our current allocations before we destroy them
                if constexpr (debug::kLeakDetectionEnabled)
                {
                    m_registry->ReportLeaks(GetName());
                }

                // Unregister from global tracker
                debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(m_registry.get());
            }
#endif

            if (m_base)
            {
                FreePages(m_base, m_capacity);
            }

            m_base = other.m_base;
            m_current = other.m_current;
            m_capacity = other.m_capacity;

#if COMB_MEM_DEBUG
            // Move the debug tracking objects
            m_registry = std::move(other.m_registry);
            m_history = std::move(other.m_history);
            m_releaseCurrent = other.m_releaseCurrent;
            // No need to re-register - global tracker still points to same registry object
#endif

            other.m_base = nullptr;
            other.m_current = nullptr;
            other.m_capacity = 0;
#if COMB_MEM_DEBUG
            other.m_releaseCurrent = nullptr;
#endif
        }

        return *this;
    }

    void* LinearAllocator::Allocate(size_t size, size_t alignment, const char* tag)
    {
#if COMB_MEM_DEBUG
        void* result = AllocateDebug(size, alignment, tag);
#else
        (void)tag; // Suppress unused warning in release

        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be a power of 2");
        hive::Assert(size > 0, "Cannot allocate 0 bytes");

        const uintptr_t currentAddr = reinterpret_cast<uintptr_t>(m_current);
        const uintptr_t aligned_addr = AlignUp(currentAddr, alignment);
        const size_t padding = aligned_addr - currentAddr;
        const size_t required = padding + size;
        const size_t used = currentAddr - reinterpret_cast<uintptr_t>(m_base);
        const size_t remaining = m_capacity - used;

        if (required > remaining)
        {
            return nullptr;
        }

        void* result = reinterpret_cast<void*>(aligned_addr);
        m_current = reinterpret_cast<void*>(aligned_addr + size);
#endif

        return result;
    }

    void LinearAllocator::Deallocate(void* ptr)
    {
#if COMB_MEM_DEBUG
        DeallocateDebug(ptr);
#else
        (void)ptr; // LinearAllocator doesn't support individual deallocation
#endif
    }

    void LinearAllocator::Reset()
    {
        m_current = m_base;

#if COMB_MEM_DEBUG
        m_releaseCurrent = m_base; // Reset virtual release pointer

        // Clear debug tracking registry
        if (m_registry)
        {
            m_registry->Clear();
        }
#endif
    }

    void* LinearAllocator::GetMarker() const
    {
        return m_current;
    }

    void LinearAllocator::ResetToMarker(void* marker)
    {
        const uintptr_t markerAddr = reinterpret_cast<uintptr_t>(marker);
        const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_base);
        const uintptr_t endAddr = baseAddr + m_capacity;

        hive::Assert(markerAddr >= baseAddr && markerAddr <= endAddr, "Marker is outside allocator memory range");

        m_current = marker;

#if COMB_MEM_DEBUG
        // Recalculate virtual release pointer based on allocations before marker
        if (m_registry)
        {
            // Sum up user bytes + padding for all allocations before marker
            // This recreates what m_releaseCurrent would be at this marker position
            size_t releaseOffset = m_registry->CalculateBytesUsedUpTo(marker);
            m_releaseCurrent = static_cast<std::byte*>(m_base) + releaseOffset;

            // Clear allocations from marker onwards
            m_registry->ClearAllocationsFrom(marker);
        }
#endif
    }

    size_t LinearAllocator::GetUsedMemory() const
    {
#if COMB_MEM_DEBUG
        // In debug mode, return virtual release pointer offset
        // This tracks what m_current would be without guard bytes
        return reinterpret_cast<uintptr_t>(m_releaseCurrent) - reinterpret_cast<uintptr_t>(m_base);
#else
        // In release mode, return actual bytes consumed (includes padding naturally)
        return reinterpret_cast<uintptr_t>(m_current) - reinterpret_cast<uintptr_t>(m_base);
#endif
    }

    size_t LinearAllocator::GetTotalMemory() const
    {
        return m_capacity;
    }

    const char* LinearAllocator::GetName() const
    {
        return "LinearAllocator";
    }

#if COMB_MEM_DEBUG
    // ========================================================================
    // Debug Implementation (Only compiled when COMB_MEM_DEBUG=1)
    // ========================================================================

    void* LinearAllocator::AllocateDebug(size_t size, size_t alignment, const char* tag)
    {
        using namespace debug;

        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be a power of 2");
        hive::Assert(size > 0, "Cannot allocate 0 bytes");

        // 1. Calculate space needed (user size + guard bytes)
        const size_t guardSize = sizeof(uint32_t);
        const size_t totalSize = size + 2 * guardSize;

        // 2. Align the USER pointer (after front guard), not the raw pointer
        //    Layout: [GUARD_FRONT (4B)][user data (aligned)][GUARD_BACK (4B)]
        const uintptr_t currentAddr = reinterpret_cast<uintptr_t>(m_current);
        const uintptr_t userAddrUnaligned = currentAddr + guardSize;
        const uintptr_t userAddrAligned = AlignUp(userAddrUnaligned, alignment);
        const uintptr_t rawAddr = userAddrAligned - guardSize;

        const size_t padding = rawAddr - currentAddr;
        const size_t required = padding + totalSize;
        const size_t used = currentAddr - reinterpret_cast<uintptr_t>(m_base);
        const size_t remaining = m_capacity - used;

        if (required > remaining)
        {
            hive::LogError(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Allocation failed: size={}, alignment={}, tag={}",
                           GetName(), size, alignment, tag ? tag : "<no tag>");
            return nullptr;
        }

        void* rawPtr = reinterpret_cast<void*>(rawAddr);
        m_current = reinterpret_cast<void*>(rawAddr + totalSize);

        // Advance virtual release pointer as if we were in release mode (no guard bytes)
        const uintptr_t releaseAddr = reinterpret_cast<uintptr_t>(m_releaseCurrent);
        const uintptr_t releaseAligned = AlignUp(releaseAddr, alignment);
        m_releaseCurrent = reinterpret_cast<void*>(releaseAligned + size);

        // 3. Write guard bytes
        WriteGuard(rawPtr);

        void* userPtr = reinterpret_cast<void*>(userAddrAligned);

        WriteGuard(static_cast<std::byte*>(userPtr) + size);

        // 4. Initialize memory with pattern (detect uninitialized reads)
        if constexpr (kMemDebugEnabled)
        {
            std::memset(userPtr, allocatedMemoryPattern, size);
        }

        // 5. Register allocation
        AllocationInfo info{};
        info.m_address = userPtr;
        info.m_size = size;
        info.m_alignment = alignment;
        info.m_timestamp = GetTimestamp();
        info.m_tag = tag;
        info.m_allocationId = m_registry->GetNextAllocationId();
        info.m_threadId = GetThreadId();

#if COMB_MEM_DEBUG_CALLSTACKS
        CaptureCallstack(info.callstack, info.callstackDepth);
#endif

        m_registry->RegisterAllocation(info);

        // 6. Record in history
#if COMB_MEM_DEBUG_HISTORY
        m_history->RecordAllocation(info);
#endif

        return userPtr;
    }

    void LinearAllocator::DeallocateDebug(void* ptr)
    {
        using namespace debug;

        // LinearAllocator doesn't support individual deallocation
        // But we still track it for debugging purposes

        if (!ptr)
            return;

        // 1. Find allocation info
        auto* info = m_registry->FindAllocation(ptr);
        if (!info)
        {
            hive::LogWarning(comb::LOG_COMB_ROOT, "[MEM_DEBUG] [{}] Deallocate called on unknown pointer: {}",
                             GetName(), ptr);
            return;
        }

        // 2. Check guard bytes
        if constexpr (kMemDebugEnabled)
        {
            if (!info->CheckGuards())
            {
                if (info->ReadGuardFront() != guardMagic)
                {
                    hive::LogError(comb::LOG_COMB_ROOT,
                                   "[MEM_DEBUG] [{}] Buffer UNDERRUN detected! Address: {}, Size: {}, Tag: {}",
                                   GetName(), ptr, info->m_size, info->GetTagOrDefault());
                    hive::Assert(false, "Buffer underrun detected");
                }

                if (info->ReadGuardBack() != guardMagic)
                {
                    hive::LogError(comb::LOG_COMB_ROOT,
                                   "[MEM_DEBUG] [{}] Buffer OVERRUN detected! Address: {}, Size: {}, Tag: {}",
                                   GetName(), ptr, info->m_size, info->GetTagOrDefault());
                    hive::Assert(false, "Buffer overrun detected");
                }
            }
        }

        // 3. Fill with freed pattern (detect use-after-free)
#if COMB_MEM_DEBUG_USE_AFTER_FREE
        std::memset(ptr, freedMemoryPattern, info->m_size);
#endif

        // 4. Record deallocation in history
#if COMB_MEM_DEBUG_HISTORY
        m_history->RecordDeallocation(ptr, info->m_size);
#endif

        // 5. Unregister allocation
        m_registry->UnregisterAllocation(ptr);

        // NOTE: LinearAllocator doesn't actually free individual allocations
        // Memory is only freed on Reset() or destruction
    }

#endif // COMB_MEM_DEBUG
} // namespace comb
