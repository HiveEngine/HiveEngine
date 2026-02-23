#pragma once

#include <comb/allocator_concepts.h>
#include <atomic>
#include <optional>
#include <cstdint>

namespace queen
{
    /**
     * Lock-free bounded Multi-Producer Multi-Consumer (MPMC) queue
     *
     * Provides a thread-safe queue where any thread can push or pop.
     * Uses a circular buffer with sequence numbers for lock-free operation.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ buffer_: Cell* - circular buffer of cells                      │
     * │ capacity_: size_t - buffer size (must be power of 2)           │
     * │ mask_: size_t - capacity_ - 1 for efficient modulo             │
     * │ head_: atomic<size_t> - consumer position                      │
     * │ tail_: atomic<size_t> - producer position                      │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Each cell contains:
     * - sequence: atomic<size_t> - for synchronization
     * - data: T - the stored item
     *
     * Algorithm (Dmitry Vyukov's bounded MPMC queue):
     * - Producers increment tail, write data, then update sequence
     * - Consumers wait for sequence, read data, then increment head
     *
     * Performance characteristics:
     * - Push: O(1) - CAS on tail
     * - Pop: O(1) - CAS on head
     * - Both operations are wait-free in the uncontended case
     *
     * Limitations:
     * - Fixed capacity (cannot grow)
     * - Returns false on full queue (non-blocking)
     * - Capacity must be power of 2
     *
     * @tparam T Element type
     * @tparam Allocator Memory allocator type
     */
    template<typename T, comb::Allocator Allocator>
    class MPMCQueue
    {
    public:
        explicit MPMCQueue(Allocator& allocator, size_t capacity)
            : allocator_{&allocator}
            , capacity_{RoundUpPowerOf2(capacity)}
            , mask_{capacity_ - 1}
            , head_{0}
            , tail_{0}
        {
            void* mem = allocator_->Allocate(sizeof(Cell) * capacity_, alignof(Cell));
            buffer_ = static_cast<Cell*>(mem);

            for (size_t i = 0; i < capacity_; ++i)
            {
                buffer_[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        ~MPMCQueue()
        {
            if (buffer_ != nullptr)
            {
                size_t head = head_.load(std::memory_order_relaxed);
                size_t tail = tail_.load(std::memory_order_relaxed);

                while (head != tail)
                {
                    size_t pos = head & mask_;
                    buffer_[pos].data.~T();
                    ++head;
                }

                allocator_->Deallocate(buffer_);
            }
        }

        MPMCQueue(const MPMCQueue&) = delete;
        MPMCQueue& operator=(const MPMCQueue&) = delete;
        MPMCQueue(MPMCQueue&&) = delete;
        MPMCQueue& operator=(MPMCQueue&&) = delete;

        /**
         * Push an item to the queue
         *
         * Thread-safe: Any thread can call this.
         *
         * @param item Item to push
         * @return true if successful, false if queue is full
         */
        bool Push(const T& item)
        {
            Cell* cell;
            size_t pos = tail_.load(std::memory_order_relaxed);

            for (;;)
            {
                cell = &buffer_[pos & mask_];
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0)
                {
                    // Cell is ready for writing
                    if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        break;
                    }
                }
                else if (diff < 0)
                {
                    // Queue is full
                    return false;
                }
                else
                {
                    // Another producer got ahead, retry
                    pos = tail_.load(std::memory_order_relaxed);
                }
            }

            // Write data and update sequence
            new (&cell->data) T{item};
            cell->sequence.store(pos + 1, std::memory_order_release);
            return true;
        }

        /**
         * Pop an item from the queue
         *
         * Thread-safe: Any thread can call this.
         *
         * @return The popped item, or std::nullopt if queue is empty
         */
        [[nodiscard]] std::optional<T> Pop()
        {
            Cell* cell;
            size_t pos = head_.load(std::memory_order_relaxed);

            for (;;)
            {
                cell = &buffer_[pos & mask_];
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                if (diff == 0)
                {
                    // Cell has data ready
                    if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        break;
                    }
                }
                else if (diff < 0)
                {
                    // Queue is empty
                    return std::nullopt;
                }
                else
                {
                    // Another consumer got ahead, retry
                    pos = head_.load(std::memory_order_relaxed);
                }
            }

            // Read data and update sequence
            T result = std::move(cell->data);
            cell->data.~T();
            cell->sequence.store(pos + mask_ + 1, std::memory_order_release);
            return result;
        }

        /**
         * Check if the queue is empty
         *
         * This is a snapshot and may not be accurate in concurrent context.
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            size_t head = head_.load(std::memory_order_acquire);
            size_t tail = tail_.load(std::memory_order_acquire);
            return head == tail;
        }

        /**
         * Get approximate size
         *
         * This is a snapshot and may not be accurate in concurrent context.
         */
        [[nodiscard]] size_t Size() const noexcept
        {
            size_t head = head_.load(std::memory_order_acquire);
            size_t tail = tail_.load(std::memory_order_acquire);
            return tail >= head ? tail - head : 0;
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return capacity_;
        }

    private:
        struct Cell
        {
            std::atomic<size_t> sequence;
            T data;
        };

        static size_t RoundUpPowerOf2(size_t n)
        {
            if (n == 0) return 1;
            --n;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            n |= n >> 32;
            return n + 1;
        }

        Allocator* allocator_;
        Cell* buffer_;
        size_t capacity_;
        size_t mask_;

        // Separate cache lines to avoid false sharing
        alignas(64) std::atomic<size_t> head_;
        alignas(64) std::atomic<size_t> tail_;
    };
}
