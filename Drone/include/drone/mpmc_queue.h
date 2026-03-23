#pragma once

#include <comb/allocator_concepts.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <utility>

namespace drone
{
    /**
     * Lock-free bounded Multi-Producer Multi-Consumer (MPMC) queue
     *
     * Provides a thread-safe queue where any thread can push or pop.
     * Uses a circular buffer with sequence numbers for lock-free operation.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ m_buffer: Cell* - circular buffer of cells                     │
     * │ m_capacity: size_t - buffer size (must be power of 2)          │
     * │ m_mask: size_t - m_capacity - 1 for efficient modulo           │
     * │ m_head: atomic<size_t> - consumer position                      │
     * │ m_tail: atomic<size_t> - producer position                      │
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
    template <typename T, comb::Allocator Allocator> class MPMCQueue
    {
    public:
        explicit MPMCQueue(Allocator& allocator, size_t capacity)
            : m_allocator{&allocator}
            , m_capacity{RoundUpPowerOf2(capacity)}
            , m_mask{m_capacity - 1}
            , m_head{0}
            , m_tail{0}
        {
            void* mem = m_allocator->Allocate(sizeof(Cell) * m_capacity, alignof(Cell));
            m_buffer = static_cast<Cell*>(mem);

            for (size_t i = 0; i < m_capacity; ++i)
            {
                m_buffer[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        ~MPMCQueue()
        {
            if (m_buffer != nullptr)
            {
                size_t head = m_head.load(std::memory_order_relaxed);
                size_t tail = m_tail.load(std::memory_order_relaxed);

                while (head != tail)
                {
                    size_t pos = head & m_mask;
                    m_buffer[pos].data.~T();
                    ++head;
                }

                m_allocator->Deallocate(m_buffer);
            }
        }

        MPMCQueue(const MPMCQueue&) = delete;
        MPMCQueue& operator=(const MPMCQueue&) = delete;
        MPMCQueue(MPMCQueue&&) = delete;
        MPMCQueue& operator=(MPMCQueue&&) = delete;

        // Returns false if queue is full.
        bool Push(const T& item)
        {
            Cell* cell;
            size_t pos = m_tail.load(std::memory_order_relaxed);

            for (;;)
            {
                cell = &m_buffer[pos & m_mask];
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0)
                {
                    // Cell is ready for writing
                    if (m_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
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
                    pos = m_tail.load(std::memory_order_relaxed);
                }
            }

            new (&cell->data) T{item};
            cell->sequence.store(pos + 1, std::memory_order_release);
            return true;
        }

        [[nodiscard]] std::optional<T> Pop()
        {
            Cell* cell;
            size_t pos = m_head.load(std::memory_order_relaxed);

            for (;;)
            {
                cell = &m_buffer[pos & m_mask];
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                if (diff == 0)
                {
                    // Cell has data ready
                    if (m_head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
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
                    pos = m_head.load(std::memory_order_relaxed);
                }
            }

            T result = std::move(cell->data);
            cell->data.~T();
            cell->sequence.store(pos + m_mask + 1, std::memory_order_release);
            return result;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            size_t head = m_head.load(std::memory_order_acquire);
            size_t tail = m_tail.load(std::memory_order_acquire);
            return head == tail;
        }

        [[nodiscard]] size_t Size() const noexcept
        {
            size_t head = m_head.load(std::memory_order_acquire);
            size_t tail = m_tail.load(std::memory_order_acquire);
            return tail >= head ? tail - head : 0;
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return m_capacity;
        }

    private:
        struct Cell
        {
            std::atomic<size_t> sequence;
            T data;
        };

        static size_t RoundUpPowerOf2(size_t n)
        {
            if (n == 0)
                return 1;
            --n;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            n |= n >> 32;
            return n + 1;
        }

        static constexpr size_t kCacheLineSize = 64;

        Allocator* m_allocator;
        Cell* m_buffer;
        size_t m_capacity;
        size_t m_mask;

        std::atomic<size_t> m_head;
        char m_padHead[kCacheLineSize - sizeof(std::atomic<size_t>)];
        std::atomic<size_t> m_tail;
        char m_padTail[kCacheLineSize - sizeof(std::atomic<size_t>)];

    };
} // namespace drone
