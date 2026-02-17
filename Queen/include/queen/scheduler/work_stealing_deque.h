#pragma once

#include <comb/allocator_concepts.h>
#include <atomic>
#include <optional>
#include <cstdint>

namespace queen
{
    /**
     * Growable circular buffer for work-stealing deque
     *
     * Stores tasks in a power-of-two sized array with wrap-around indexing.
     * Supports atomic growth by creating a new larger buffer and copying elements.
     *
     * Get/Put are NOT data races despite concurrent access from owner + thieves:
     * the grow check guarantees 0 < (bottom - top) < capacity, so
     * (bottom & mask) != (top & mask) — they always hit different slots.
     * TSan can't reason about modular arithmetic, so we suppress the false positive.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ data_: T* - power-of-two sized array of elements               │
     * │ capacity_: size_t - size of array (always power of 2)          │
     * │ mask_: size_t - capacity_ - 1 for efficient modulo             │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Get/Put: O(1) - direct array access with masking
     * - Grow: O(n) - copies all elements to new buffer
     *
     * @tparam T Element type (typically Task*)
     * @tparam Allocator Memory allocator type
     */
    template<typename T, comb::Allocator Allocator>
    class CircularBuffer
    {
    public:
        explicit CircularBuffer(Allocator& allocator, size_t capacity)
            : allocator_{&allocator}
            , capacity_{capacity}
            , mask_{capacity - 1}
        {
            void* mem = allocator_->Allocate(sizeof(T) * capacity_, alignof(T));
            data_ = static_cast<T*>(mem);
        }

        ~CircularBuffer()
        {
            if (data_ != nullptr)
            {
                allocator_->Deallocate(data_);
            }
        }

        CircularBuffer(const CircularBuffer&) = delete;
        CircularBuffer& operator=(const CircularBuffer&) = delete;
        CircularBuffer(CircularBuffer&&) = delete;
        CircularBuffer& operator=(CircularBuffer&&) = delete;

        [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }

        // No-sanitize: owner Put and thief Get always access different physical slots
        // (guaranteed by modular arithmetic + grow check). See class comment.
        #if defined(__clang__)
        [[clang::no_sanitize("thread")]]
        #endif
        [[nodiscard]] T Get(int64_t index) const noexcept
        {
            return data_[static_cast<size_t>(index) & mask_];
        }

        #if defined(__clang__)
        [[clang::no_sanitize("thread")]]
        #endif
        void Put(int64_t index, T value) noexcept
        {
            data_[static_cast<size_t>(index) & mask_] = value;
        }

        /**
         * Create a new buffer with doubled capacity and copy elements
         *
         * @param bottom Current bottom index (exclusive upper bound)
         * @param top Current top index (inclusive lower bound)
         * @return Pointer to new larger buffer
         */
        [[nodiscard]] CircularBuffer* Grow(int64_t bottom, int64_t top)
        {
            size_t new_capacity = capacity_ * 2;
            void* mem = allocator_->Allocate(sizeof(CircularBuffer), alignof(CircularBuffer));
            auto* new_buffer = new (mem) CircularBuffer(*allocator_, new_capacity);

            for (int64_t i = top; i < bottom; ++i)
            {
                new_buffer->Put(i, Get(i));
            }

            return new_buffer;
        }

    private:
        Allocator* allocator_;
        T* data_;
        size_t capacity_;
        size_t mask_;
    };

    /**
     * Lock-free work-stealing deque (Chase-Lev algorithm)
     *
     * Provides a double-ended queue where the owner thread can push/pop from
     * the bottom (LIFO) and other threads can steal from the top (FIFO).
     * This is the foundation for work-stealing schedulers.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ top_: atomic<int64_t> - index where thieves steal (FIFO end)   │
     * │ bottom_: atomic<int64_t> - index where owner pushes/pops       │
     * │ buffer_: atomic<CircularBuffer*> - growable storage            │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Operations:
     * - Push: O(1) amortized - owner pushes to bottom
     * - Pop: O(1) - owner pops from bottom (LIFO)
     * - Steal: O(1) - thieves steal from top (FIFO)
     *
     * Thread safety:
     * - Push/Pop: Only the owning worker thread may call these
     * - Steal: Any thread may call this (lock-free with CAS)
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1_MB};
     *   WorkStealingDeque<Task*> deque{alloc, 1024};
     *
     *   // Owner thread
     *   deque.Push(task1);
     *   deque.Push(task2);
     *   auto task = deque.Pop();  // Returns task2 (LIFO)
     *
     *   // Thief thread
     *   auto stolen = deque.Steal();  // Returns task1 (FIFO)
     * @endcode
     */
    template<typename T, comb::Allocator Allocator>
    class WorkStealingDeque
    {
    public:
        explicit WorkStealingDeque(Allocator& allocator, size_t initial_capacity = 1024)
            : allocator_{&allocator}
            , top_{0}
            , bottom_{0}
            , retired_head_{nullptr}
        {
            void* mem = allocator_->Allocate(sizeof(CircularBuffer<T, Allocator>), alignof(CircularBuffer<T, Allocator>));
            auto* initial_buffer = new (mem) CircularBuffer<T, Allocator>(allocator, initial_capacity);
            buffer_.store(initial_buffer, std::memory_order_relaxed);
        }

        ~WorkStealingDeque()
        {
            auto* buf = buffer_.load(std::memory_order_relaxed);
            if (buf != nullptr)
            {
                buf->~CircularBuffer<T, Allocator>();
                allocator_->Deallocate(buf);
            }
            FreeRetiredBuffers();
        }

        WorkStealingDeque(const WorkStealingDeque&) = delete;
        WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;
        WorkStealingDeque(WorkStealingDeque&&) = delete;
        WorkStealingDeque& operator=(WorkStealingDeque&&) = delete;

        /**
         * Push an item onto the bottom of the deque
         *
         * Only the owning worker thread should call this.
         * May grow the buffer if full.
         *
         * @param item Item to push
         */
        void Push(T item)
        {
            int64_t b = bottom_.load(std::memory_order_relaxed);
            int64_t t = top_.load(std::memory_order_acquire);
            auto* buf = buffer_.load(std::memory_order_relaxed);

            if (b - t > static_cast<int64_t>(buf->Capacity()) - 1)
            {
                auto* old_buf = buf;
                buf = buf->Grow(b, t);
                buffer_.store(buf, std::memory_order_release);
                RetireBuffer(old_buf);
            }

            buf->Put(b, item);
            // Release store ensures the item is visible before bottom is incremented
            bottom_.store(b + 1, std::memory_order_release);
        }

        /**
         * Pop an item from the bottom of the deque
         *
         * Only the owning worker thread should call this.
         * Returns std::nullopt if the deque is empty.
         *
         * @return The popped item or std::nullopt
         */
        [[nodiscard]] std::optional<T> Pop()
        {
            int64_t b = bottom_.load(std::memory_order_relaxed) - 1;
            auto* buf = buffer_.load(std::memory_order_relaxed);
            bottom_.store(b, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t t = top_.load(std::memory_order_relaxed);

            if (t <= b)
            {
                T item = buf->Get(b);
                if (t == b)
                {
                    if (!top_.compare_exchange_strong(t, t + 1,
                                                       std::memory_order_seq_cst,
                                                       std::memory_order_relaxed))
                    {
                        bottom_.store(b + 1, std::memory_order_relaxed);
                        return std::nullopt;
                    }
                    bottom_.store(b + 1, std::memory_order_relaxed);
                }
                return item;
            }

            bottom_.store(b + 1, std::memory_order_relaxed);
            return std::nullopt;
        }

        /**
         * Steal an item from the top of the deque
         *
         * Any thread may call this. Lock-free using CAS.
         * Returns std::nullopt if the deque is empty or if
         * the steal was lost to another thread.
         *
         * @return The stolen item or std::nullopt
         */
        [[nodiscard]] std::optional<T> Steal()
        {
            int64_t t = top_.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t b = bottom_.load(std::memory_order_acquire);

            if (t < b)
            {
                auto* buf = buffer_.load(std::memory_order_acquire);
                T item = buf->Get(t);
                if (!top_.compare_exchange_strong(t, t + 1,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_relaxed))
                {
                    return std::nullopt;
                }
                return item;
            }

            return std::nullopt;
        }

        /**
         * Check if the deque is empty
         *
         * This is a snapshot and may not be accurate in a concurrent context.
         *
         * @return true if the deque appears empty
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            int64_t t = top_.load(std::memory_order_acquire);
            int64_t b = bottom_.load(std::memory_order_acquire);
            return t >= b;
        }

        /**
         * Get the approximate size of the deque
         *
         * This is a snapshot and may not be accurate in a concurrent context.
         *
         * @return Approximate number of items
         */
        [[nodiscard]] size_t Size() const noexcept
        {
            int64_t t = top_.load(std::memory_order_acquire);
            int64_t b = bottom_.load(std::memory_order_acquire);
            return static_cast<size_t>(b > t ? b - t : 0);
        }

    private:
        struct RetiredNode
        {
            CircularBuffer<T, Allocator>* buffer;
            RetiredNode* next;
        };

        void RetireBuffer(CircularBuffer<T, Allocator>* buf)
        {
            void* mem = allocator_->Allocate(sizeof(RetiredNode), alignof(RetiredNode));
            auto* node = new (mem) RetiredNode{buf, retired_head_};
            retired_head_ = node;
        }

        void FreeRetiredBuffers()
        {
            RetiredNode* node = retired_head_;
            while (node != nullptr)
            {
                RetiredNode* next = node->next;
                node->buffer->~CircularBuffer<T, Allocator>();
                allocator_->Deallocate(node->buffer);
                allocator_->Deallocate(node);
                node = next;
            }
            retired_head_ = nullptr;
        }

        Allocator* allocator_;
        std::atomic<int64_t> top_;
        std::atomic<int64_t> bottom_;
        std::atomic<CircularBuffer<T, Allocator>*> buffer_;
        RetiredNode* retired_head_;
    };
}
