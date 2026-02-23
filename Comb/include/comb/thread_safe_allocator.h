#pragma once

#include <comb/allocator_concepts.h>
#include <hive/profiling/profiler.h>
#include <mutex>

namespace comb
{
    /**
     * Thread-safe allocator wrapper
     *
     * Wraps any allocator to make it thread-safe by protecting
     * Allocate/Deallocate calls with a mutex.
     *
     * Use cases:
     * - Sharing an allocator across multiple threads
     * - Thread pools that need concurrent allocations
     * - Any situation where multiple threads access the same allocator
     *
     * Performance characteristics:
     * - Allocation: Base allocator time + mutex lock/unlock (~50ns overhead)
     * - Deallocation: Base allocator time + mutex lock/unlock (~50ns overhead)
     * - Contention: High contention will degrade performance
     *
     * Limitations:
     * - Adds mutex overhead to every allocation
     * - Not suitable for high-frequency allocations from many threads
     * - Consider per-thread allocators for better performance
     *
     * Example:
     * @code
     *   comb::BuddyAllocator buddy{10_MB};
     *   comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};
     *
     *   // Can now safely use from multiple threads
     *   std::thread t1([&]() { void* p = safe.Allocate(64, 8); });
     *   std::thread t2([&]() { void* p = safe.Allocate(128, 8); });
     * @endcode
     *
     * @tparam Allocator The underlying allocator type (must satisfy comb::Allocator)
     */
    template<Allocator UnderlyingAllocator>
    class ThreadSafeAllocator
    {
    public:
        /**
         * Construct a thread-safe wrapper around an existing allocator
         *
         * @param allocator Reference to the underlying allocator (must outlive this wrapper)
         */
        explicit ThreadSafeAllocator(UnderlyingAllocator& allocator) noexcept
            : allocator_{&allocator}
        {}

        ~ThreadSafeAllocator() = default;

        // Non-copyable (contains mutex reference semantics)
        ThreadSafeAllocator(const ThreadSafeAllocator&) = delete;
        ThreadSafeAllocator& operator=(const ThreadSafeAllocator&) = delete;

        // Movable
        ThreadSafeAllocator(ThreadSafeAllocator&& other) noexcept
            : allocator_{other.allocator_}
        {
            other.allocator_ = nullptr;
        }

        ThreadSafeAllocator& operator=(ThreadSafeAllocator&& other) noexcept
        {
            if (this != &other)
            {
                allocator_ = other.allocator_;
                other.allocator_ = nullptr;
            }
            return *this;
        }

        /**
         * Allocate memory (thread-safe)
         *
         * @param size Number of bytes to allocate
         * @param alignment Required alignment (must be power of 2)
         * @param tag Optional allocation tag for debugging
         * @return Pointer to allocated memory, or nullptr if out of memory
         */
        [[nodiscard]] void* Allocate(size_t size, size_t alignment, const char* tag = nullptr)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{mutex_};
            return allocator_->Allocate(size, alignment, tag);
        }

        /**
         * Deallocate memory (thread-safe)
         *
         * @param ptr Pointer to deallocate
         */
        void Deallocate(void* ptr)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{mutex_};
            allocator_->Deallocate(ptr);
        }

        /**
         * Get the usable size of an allocation's block (lock-free)
         *
         * Safe to call without a lock because the block header is
         * immutable between Allocate and Deallocate.
         */
        [[nodiscard]] size_t GetBlockUsableSize(const void* ptr) const
        {
            return allocator_->GetBlockUsableSize(ptr);
        }

        /**
         * Get the underlying allocator (not thread-safe access!)
         *
         * Warning: Direct access to underlying allocator bypasses thread safety.
         * Prefer using the wrapper's methods (GetUsedMemory, GetTotalMemory) which are mutex-protected.
         * Only use Underlying() for allocator-specific methods not exposed by the wrapper.
         */
        [[nodiscard]] UnderlyingAllocator& Underlying() noexcept
        {
            return *allocator_;
        }

        [[nodiscard]] const UnderlyingAllocator& Underlying() const noexcept
        {
            return *allocator_;
        }

        /**
         * Get allocator name
         */
        [[nodiscard]] const char* GetName() const noexcept
        {
            return "ThreadSafeAllocator";
        }

        /**
         * Get used memory (delegates to underlying, may not be thread-safe depending on allocator)
         */
        [[nodiscard]] size_t GetUsedMemory() const
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{mutex_};
            return allocator_->GetUsedMemory();
        }

        /**
         * Get total memory capacity
         */
        [[nodiscard]] size_t GetTotalMemory() const
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{mutex_};
            return allocator_->GetTotalMemory();
        }

    private:
        UnderlyingAllocator* allocator_;
        mutable HIVE_PROFILE_LOCKABLE_N(std::mutex, mutex_, "AllocatorMutex");
    };
}
