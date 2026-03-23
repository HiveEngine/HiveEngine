#pragma once

#include <hive/hive_config.h>

#include <comb/allocator_concepts.h>

#include <cstddef>
#include <memory>

// Memory debugging (zero overhead when disabled)
#if COMB_MEM_DEBUG
#include <comb/debug/allocation_history.h>
#include <comb/debug/allocation_registry.h>
#include <comb/debug/global_memory_tracker.h>
#include <comb/debug/mem_debug_config.h>
#include <comb/debug/platform_utils.h>
#endif

namespace comb
{
    // Linear (bump/arena) allocator — O(1) alloc, no individual deallocation.
    // Reset() frees everything at once. Supports markers for scoped rollback.
    class HIVE_API LinearAllocator
    {
    public:
        explicit LinearAllocator(size_t capacity);

        /**
         * Destructor - frees memory back to OS
         */
        ~LinearAllocator();

        // Disable copy (allocators manage memory, shouldn't be copied)
        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;

        /**
         * Move constructor/assignment (transfer ownership)
         *
         * Transfers both memory and debug tracking to the new allocator.
         * All allocations made before the move can still be deallocated.
         */
        LinearAllocator(LinearAllocator&& other) noexcept;
        LinearAllocator& operator=(LinearAllocator&& other) noexcept;

        /**
         * Allocate aligned memory
         * @param size Number of bytes to allocate
         * @param alignment Required alignment (must be power of 2)
         * @param tag Optional allocation tag for debugging (e.g., "Player Entity")
         * @return Pointer to allocated memory, or nullptr if out of memory
         *
         * Note: tag parameter is zero-cost when COMB_MEM_DEBUG=0
         */
        [[nodiscard]] void* Allocate(size_t size, size_t alignment, const char* tag = nullptr);

        /**
         * Deallocate memory (no-op for linear allocator)
         * @param ptr Pointer to deallocate (ignored)
         */
        void Deallocate(void* ptr);

        /**
         * Reset allocator to initial state (frees all allocations)
         * Very fast - just resets current pointer to base
         */
        void Reset();

        /**
         * Get a marker representing current allocation position
         * Can be used to restore to this point later
         * @return Opaque pointer representing current position
         */
        [[nodiscard]] void* GetMarker() const noexcept;

        /**
         * Reset allocator to a previously saved marker
         * Frees all allocations made after the marker
         * @param marker Marker obtained from GetMarker()
         */
        void ResetToMarker(void* marker);

        /**
         * Get number of bytes currently allocated
         * @return Bytes allocated (difference between current and base)
         */
        [[nodiscard]] size_t GetUsedMemory() const noexcept;

        /**
         * Get total capacity of allocator
         * @return Total bytes available
         */
        [[nodiscard]] size_t GetTotalMemory() const noexcept;

        /**
         * Get allocator name for debugging
         * @return "LinearAllocator"
         */
        [[nodiscard]] const char* GetName() const noexcept;

    private:
        void* m_base{nullptr};
        void* m_current{nullptr};
        size_t m_capacity{0};

#if COMB_MEM_DEBUG
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> m_registry;
        std::unique_ptr<debug::AllocationHistory> m_history;

        // Tracks current_ without guard bytes — keeps GetUsedMemory() consistent across debug/release
        void* m_releaseCurrent{nullptr};
#endif
    };

    static_assert(Allocator<LinearAllocator>, "LinearAllocator must satisfy Allocator concept");
} // namespace comb
