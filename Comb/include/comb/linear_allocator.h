#pragma once

#include <comb/allocator_concepts.h>
#include <cstddef>
#include <memory>

// Memory debugging (zero overhead when disabled)
#if COMB_MEM_DEBUG
    #include <comb/debug/mem_debug_config.h>
    #include <comb/debug/allocation_registry.h>
    #include <comb/debug/allocation_history.h>
    #include <comb/debug/global_memory_tracker.h>
    #include <comb/debug/platform_utils.h>
#endif

namespace comb
{
    /**
     * Linear allocator (also called Arena or Bump allocator)
     *
     * Allocates memory sequentially by bumping a pointer forward.
     * Provides the fastest possible allocation with zero overhead.
     * Cannot free individual allocations, only reset all at once.
     *
     * Satisfies the comb::Allocator concept.
     *
     * Use cases:
     * - Frame-scoped allocations (reset every frame)
     * - Temporary parsing/loading data (destroyed after use)
     * - Scope-based allocations with markers
     * - Single-threaded high-frequency allocations
     *
     * Memory layout:
     * ┌──────────────────────────────────────────┐
     * │ base_           current_        capacity_│
     * │  ↓                ↓                      │
     * │  [===== Used =====][===== Free =====]    │
     * └──────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Allocation: O(1) - pointer bump
     * - Deallocation: No-op (instant, does nothing)
     * - Reset: O(1) - single pointer write (instant)
     * - Thread-safe: No (use per-thread allocators)
     * - Fragmentation: None (sequential allocation)
     *
     * Limitations:
     * - No individual deallocation (must Reset() entire arena)
     * - Memory wasted if objects have different lifetimes
     * - Not thread-safe (requires external synchronization)
     * - Fixed capacity (set at construction)
     *
     * Example:
     * @code
     *   // Create 10MB frame allocator
     *   comb::LinearAllocator frameAlloc{10 * 1024 * 1024};
     *
     *   // Allocate during frame
     *   auto* entity = comb::New<Entity>(frameAlloc);
     *   auto* buffer = frameAlloc.Allocate(1024, 16);
     *
     *   // Use marker for scoped allocation
     *   void* marker = frameAlloc.GetMarker();
     *   // ... temp allocations ...
     *   frameAlloc.ResetToMarker(marker);
     *
     *   // End of frame - reset everything
     *   frameAlloc.Reset(); // instant
     * @endcode
     */
    class LinearAllocator
    {
    public:
        /**
         * Construct a linear allocator with given capacity
         * @param capacity Total size in bytes to allocate from OS
         */
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
        [[nodiscard]] void* GetMarker() const;

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
        [[nodiscard]] size_t GetUsedMemory() const;

        /**
         * Get total capacity of allocator
         * @return Total bytes available
         */
        [[nodiscard]] size_t GetTotalMemory() const;

        /**
         * Get allocator name for debugging
         * @return "LinearAllocator"
         */
        [[nodiscard]] const char* GetName() const;

    private:
        void* base_{nullptr};
        void* current_{nullptr};
        size_t capacity_{0};

#if COMB_MEM_DEBUG
        // Debug tracking (zero overhead when COMB_MEM_DEBUG=0)
        void* AllocateDebug(size_t size, size_t alignment, const char* tag);
        void DeallocateDebug(void* ptr);

        // Use unique_ptr to enable move semantics (AllocationRegistry contains non-movable mutex)
        std::unique_ptr<debug::AllocationRegistry> registry_;
        std::unique_ptr<debug::AllocationHistory> history_;

        // Tracks current_ without guard bytes — keeps GetUsedMemory() consistent across debug/release
        void* release_current_{nullptr};
#endif
    };

    static_assert(Allocator<LinearAllocator>, "LinearAllocator must satisfy Allocator concept");
}

