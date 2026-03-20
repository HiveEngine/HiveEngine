#pragma once

#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>

#include <mutex>

namespace comb
{
    // Thread-safe allocator wrapper. Protects Allocate/Deallocate with a mutex.
    template <Allocator UnderlyingAllocator> class ThreadSafeAllocator
    {
    public:
        /**
         * Construct a thread-safe wrapper around an existing allocator
         *
         * @param allocator Reference to the underlying allocator (must outlive this wrapper)
         */
        explicit ThreadSafeAllocator(UnderlyingAllocator& allocator) noexcept
            : m_allocator{&allocator}
        {
        }

        ~ThreadSafeAllocator() = default;

        // Non-copyable (contains mutex reference semantics)
        ThreadSafeAllocator(const ThreadSafeAllocator&) = delete;
        ThreadSafeAllocator& operator=(const ThreadSafeAllocator&) = delete;

        // Movable
        ThreadSafeAllocator(ThreadSafeAllocator&& other) noexcept
            : m_allocator{other.m_allocator}
        {
            other.m_allocator = nullptr;
        }

        ThreadSafeAllocator& operator=(ThreadSafeAllocator&& other) noexcept
        {
            if (this != &other)
            {
                m_allocator = other.m_allocator;
                other.m_allocator = nullptr;
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
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_mutex};
            return m_allocator->Allocate(size, alignment, tag);
        }

        /**
         * Deallocate memory (thread-safe)
         *
         * @param ptr Pointer to deallocate
         */
        void Deallocate(void* ptr)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_mutex};
            m_allocator->Deallocate(ptr);
        }

        /**
         * Get the usable size of an allocation's block (lock-free)
         *
         * Safe to call without a lock because the block header is
         * immutable between Allocate and Deallocate.
         */
        [[nodiscard]] size_t GetBlockUsableSize(const void* ptr) const noexcept
        {
            return m_allocator->GetBlockUsableSize(ptr);
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
            return *m_allocator;
        }

        [[nodiscard]] const UnderlyingAllocator& Underlying() const noexcept
        {
            return *m_allocator;
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
        [[nodiscard]] size_t GetUsedMemory() const noexcept
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_mutex};
            return m_allocator->GetUsedMemory();
        }

        /**
         * Get total memory capacity
         */
        [[nodiscard]] size_t GetTotalMemory() const noexcept
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_mutex};
            return m_allocator->GetTotalMemory();
        }

    private:
        UnderlyingAllocator* m_allocator;
        mutable HIVE_PROFILE_LOCKABLE_N(std::mutex, m_mutex, "AllocatorMutex");
    };
} // namespace comb
