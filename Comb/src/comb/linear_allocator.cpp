#include <comb/precomp.h>
#include <comb/linear_allocator.h>
#include <comb/platform.h>
#include <comb/utils.h>
#include <hive/core/assert.h>
#include <hive/core/log.h>
#include <comb/combmodule.h>

#include <cstring>  // For memset

namespace comb
{
    LinearAllocator::LinearAllocator(size_t capacity)
        : base_{AllocatePages(capacity)}
        , current_{base_}
        , capacity_{capacity}
    {
        hive::Assert(base_ != nullptr, "Failed to allocate memory for LinearAllocator");

#if COMB_MEM_DEBUG
        // Create debug tracking objects
        registry_ = std::make_unique<debug::AllocationRegistry>();
        history_ = std::make_unique<debug::AllocationHistory>();
        release_current_ = base_;  // Initialize virtual release pointer

        // Register with global tracker
        debug::GlobalMemoryTracker::GetInstance().RegisterAllocator(GetName(), registry_.get());
#endif
    }

    LinearAllocator::~LinearAllocator()
    {
#if COMB_MEM_DEBUG
        if (registry_)
        {
            // NOTE: LinearAllocator doesn't support individual deallocation
            // Having "leaks" at destruction is NORMAL and EXPECTED behavior
            // Users should call Reset() if they want to verify no leaks, but it's not required
            // We DON'T report leaks here because it would be too noisy for normal usage

            // Unregister from global tracker
            debug::GlobalMemoryTracker::GetInstance().UnregisterAllocator(registry_.get());
        }
#endif

        if (base_)
        {
            FreePages(base_, capacity_);
            base_ = nullptr;
            current_ = nullptr;
        }
    }

    LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
        : base_{other.base_}
        , current_{other.current_}
        , capacity_{other.capacity_}
#if COMB_MEM_DEBUG
        , registry_{std::move(other.registry_)}
        , history_{std::move(other.history_)}
        , release_current_{other.release_current_}
#endif
    {
        // Note: registry_ and history_ are automatically moved via unique_ptr
        // No need to update global tracker - it still points to the same registry object

        other.base_ = nullptr;
        other.current_ = nullptr;
        other.capacity_ = 0;
#if COMB_MEM_DEBUG
        other.release_current_ = nullptr;
#endif
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept
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

            if (base_)
            {
                FreePages(base_, capacity_);
            }

            base_ = other.base_;
            current_ = other.current_;
            capacity_ = other.capacity_;

#if COMB_MEM_DEBUG
            // Move the debug tracking objects
            registry_ = std::move(other.registry_);
            history_ = std::move(other.history_);
            release_current_ = other.release_current_;
            // No need to re-register - global tracker still points to same registry object
#endif

            other.base_ = nullptr;
            other.current_ = nullptr;
            other.capacity_ = 0;
#if COMB_MEM_DEBUG
            other.release_current_ = nullptr;
#endif
        }

        return *this;
    }

    void* LinearAllocator::Allocate(size_t size, size_t alignment, const char* tag)
    {
#if COMB_MEM_DEBUG
        void* result = AllocateDebug(size, alignment, tag);
#else
        (void)tag;  // Suppress unused warning in release

        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be a power of 2");
        hive::Assert(size > 0, "Cannot allocate 0 bytes");

        const uintptr_t current_addr = reinterpret_cast<uintptr_t>(current_);
        const uintptr_t aligned_addr = AlignUp(current_addr, alignment);
        const size_t padding = aligned_addr - current_addr;
        const size_t required = padding + size;
        const size_t used = current_addr - reinterpret_cast<uintptr_t>(base_);
        const size_t remaining = capacity_ - used;

        if (required > remaining)
        {
            return nullptr;
        }

        void* result = reinterpret_cast<void*>(aligned_addr);
        current_ = reinterpret_cast<void*>(aligned_addr + size);
#endif

        return result;
    }

    void LinearAllocator::Deallocate(void* ptr)
    {
#if COMB_MEM_DEBUG
        DeallocateDebug(ptr);
#else
        (void)ptr;  // LinearAllocator doesn't support individual deallocation
#endif
    }

    void LinearAllocator::Reset()
    {
        current_ = base_;

#if COMB_MEM_DEBUG
        release_current_ = base_;  // Reset virtual release pointer

        // Clear debug tracking registry
        if (registry_)
        {
            registry_->Clear();
        }
#endif
    }

    void* LinearAllocator::GetMarker() const
    {
        return current_;
    }

    void LinearAllocator::ResetToMarker(void* marker)
    {
        const uintptr_t marker_addr = reinterpret_cast<uintptr_t>(marker);
        const uintptr_t base_addr = reinterpret_cast<uintptr_t>(base_);
        const uintptr_t end_addr = base_addr + capacity_;

        hive::Assert(marker_addr >= base_addr && marker_addr <= end_addr,
                     "Marker is outside allocator memory range");

        current_ = marker;

#if COMB_MEM_DEBUG
        // Recalculate virtual release pointer based on allocations before marker
        if (registry_)
        {
            // Sum up user bytes + padding for all allocations before marker
            // This recreates what release_current_ would be at this marker position
            size_t release_offset = registry_->CalculateBytesUsedUpTo(marker);
            release_current_ = static_cast<std::byte*>(base_) + release_offset;

            // Clear allocations from marker onwards
            registry_->ClearAllocationsFrom(marker);
        }
#endif
    }

    size_t LinearAllocator::GetUsedMemory() const
    {
#if COMB_MEM_DEBUG
        // In debug mode, return virtual release pointer offset
        // This tracks what current_ would be without guard bytes
        return reinterpret_cast<uintptr_t>(release_current_) - reinterpret_cast<uintptr_t>(base_);
#else
        // In release mode, return actual bytes consumed (includes padding naturally)
        return reinterpret_cast<uintptr_t>(current_) - reinterpret_cast<uintptr_t>(base_);
#endif
    }

    size_t LinearAllocator::GetTotalMemory() const
    {
        return capacity_;
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
        const uintptr_t current_addr = reinterpret_cast<uintptr_t>(current_);
        const uintptr_t user_addr_unaligned = current_addr + guardSize;
        const uintptr_t user_addr_aligned = AlignUp(user_addr_unaligned, alignment);
        const uintptr_t raw_addr = user_addr_aligned - guardSize;

        const size_t padding = raw_addr - current_addr;
        const size_t required = padding + totalSize;
        const size_t used = current_addr - reinterpret_cast<uintptr_t>(base_);
        const size_t remaining = capacity_ - used;

        if (required > remaining)
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] [{}] Allocation failed: size={}, alignment={}, tag={}",
                           GetName(), size, alignment, tag ? tag : "<no tag>");
            return nullptr;
        }

        void* rawPtr = reinterpret_cast<void*>(raw_addr);
        current_ = reinterpret_cast<void*>(raw_addr + totalSize);

        // Advance virtual release pointer as if we were in release mode (no guard bytes)
        const uintptr_t release_addr = reinterpret_cast<uintptr_t>(release_current_);
        const uintptr_t release_aligned = AlignUp(release_addr, alignment);
        release_current_ = reinterpret_cast<void*>(release_aligned + size);

        // 3. Write guard bytes
        WriteGuard(rawPtr);

        void* userPtr = reinterpret_cast<void*>(user_addr_aligned);

        WriteGuard(static_cast<std::byte*>(userPtr) + size);

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

    void LinearAllocator::DeallocateDebug(void* ptr)
    {
        using namespace debug;

        // LinearAllocator doesn't support individual deallocation
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
        std::memset(ptr, FreedMemoryPattern, info->size);
#endif

        // 4. Record deallocation in history
#if COMB_MEM_DEBUG_HISTORY
        history_->RecordDeallocation(ptr, info->size);
#endif

        // 5. Unregister allocation
        registry_->UnregisterAllocation(ptr);

        // NOTE: LinearAllocator doesn't actually free individual allocations
        // Memory is only freed on Reset() or destruction
    }

#endif // COMB_MEM_DEBUG
}
