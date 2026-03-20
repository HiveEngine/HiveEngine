#pragma once

#include <comb/allocator_concepts.h>

#include <atomic>
#include <cstdint>
#include <optional>

namespace drone
{
    // Get/Put are NOT data races despite concurrent access from owner + thieves:
    // the grow check guarantees 0 < (bottom - top) < capacity, so
    // (bottom & mask) != (top & mask) — they always hit different slots.
    // TSan can't reason about modular arithmetic, so we suppress the false positive.
    template <typename T, comb::Allocator Allocator> class CircularBuffer
    {
    public:
        explicit CircularBuffer(Allocator& allocator, size_t capacity)
            : m_allocator{&allocator}
            , m_capacity{capacity}
            , m_mask{capacity - 1}
        {
            void* mem = m_allocator->Allocate(sizeof(T) * m_capacity, alignof(T));
            m_data = static_cast<T*>(mem);
        }

        ~CircularBuffer()
        {
            if (m_data != nullptr)
            {
                m_allocator->Deallocate(m_data);
            }
        }

        CircularBuffer(const CircularBuffer&) = delete;
        CircularBuffer& operator=(const CircularBuffer&) = delete;
        CircularBuffer(CircularBuffer&&) = delete;
        CircularBuffer& operator=(CircularBuffer&&) = delete;

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return m_capacity;
        }

// No-sanitize: owner Put and thief Get always access different physical slots
// (guaranteed by modular arithmetic + grow check). See class comment.
#if defined(__clang__)
        [[clang::no_sanitize("thread")]]
#endif
        [[nodiscard]] T
        Get(int64_t index) const noexcept
        {
            return m_data[static_cast<size_t>(index) & m_mask];
        }

#if defined(__clang__)
        [[clang::no_sanitize("thread")]]
#endif
        void Put(int64_t index, T value) noexcept
        {
            m_data[static_cast<size_t>(index) & m_mask] = value;
        }

        [[nodiscard]] CircularBuffer* Grow(int64_t bottom, int64_t top)
        {
            size_t newCapacity = m_capacity * 2;
            void* mem = m_allocator->Allocate(sizeof(CircularBuffer), alignof(CircularBuffer));
            auto* newBuffer = new (mem) CircularBuffer{*m_allocator, newCapacity};

            for (int64_t i = top; i < bottom; ++i)
            {
                newBuffer->Put(i, Get(i));
            }

            return newBuffer;
        }

    private:
        Allocator* m_allocator;
        T* m_data;
        size_t m_capacity;
        size_t m_mask;
    };

    // Chase-Lev work-stealing deque. Push/Pop: owner thread only (LIFO).
    // Steal: any thread (FIFO, lock-free CAS). Ref: Le et al., PPoPP 2013.
    template <typename T, comb::Allocator Allocator> class WorkStealingDeque
    {
    public:
        explicit WorkStealingDeque(Allocator& allocator, size_t initialCapacity = 1024)
            : m_allocator{&allocator}
            , m_top{0}
            , m_bottom{0}
            , m_retiredHead{nullptr}
        {
            void* mem =
                m_allocator->Allocate(sizeof(CircularBuffer<T, Allocator>), alignof(CircularBuffer<T, Allocator>));
            auto* initialBuffer = new (mem) CircularBuffer<T, Allocator>{allocator, initialCapacity};
            m_buffer.store(initialBuffer, std::memory_order_relaxed);
        }

        ~WorkStealingDeque()
        {
            auto* buf = m_buffer.load(std::memory_order_relaxed);
            if (buf != nullptr)
            {
                buf->~CircularBuffer<T, Allocator>();
                m_allocator->Deallocate(buf);
            }
            FreeRetiredBuffers();
        }

        WorkStealingDeque(const WorkStealingDeque&) = delete;
        WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;
        WorkStealingDeque(WorkStealingDeque&&) = delete;
        WorkStealingDeque& operator=(WorkStealingDeque&&) = delete;

        void Push(T item)
        {
            int64_t b = m_bottom.load(std::memory_order_relaxed);
            int64_t t = m_top.load(std::memory_order_acquire);
            auto* buf = m_buffer.load(std::memory_order_relaxed);

            if (b - t > static_cast<int64_t>(buf->Capacity()) - 1)
            {
                auto* oldBuf = buf;
                buf = buf->Grow(b, t);
                m_buffer.store(buf, std::memory_order_release);
                RetireBuffer(oldBuf);
            }

            buf->Put(b, item);
            m_bottom.store(b + 1, std::memory_order_release);
        }

        [[nodiscard]] std::optional<T> Pop()
        {
            int64_t b = m_bottom.load(std::memory_order_relaxed) - 1;
            auto* buf = m_buffer.load(std::memory_order_relaxed);
            m_bottom.store(b, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t t = m_top.load(std::memory_order_relaxed);

            if (t <= b)
            {
                T item = buf->Get(b);
                if (t == b)
                {
                    if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
                    {
                        m_bottom.store(b + 1, std::memory_order_relaxed);
                        return std::nullopt;
                    }
                    m_bottom.store(b + 1, std::memory_order_relaxed);
                }
                return item;
            }

            m_bottom.store(b + 1, std::memory_order_relaxed);
            return std::nullopt;
        }

        [[nodiscard]] std::optional<T> Steal()
        {
            int64_t t = m_top.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t b = m_bottom.load(std::memory_order_acquire);

            if (t < b)
            {
                auto* buf = m_buffer.load(std::memory_order_acquire);
                T item = buf->Get(t);
                if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
                {
                    return std::nullopt;
                }
                return item;
            }

            return std::nullopt;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            int64_t t = m_top.load(std::memory_order_acquire);
            int64_t b = m_bottom.load(std::memory_order_acquire);
            return t >= b;
        }

        [[nodiscard]] size_t Size() const noexcept
        {
            int64_t t = m_top.load(std::memory_order_acquire);
            int64_t b = m_bottom.load(std::memory_order_acquire);
            return static_cast<size_t>(b > t ? b - t : 0);
        }

    private:
        struct RetiredNode
        {
            CircularBuffer<T, Allocator>* m_buffer;
            RetiredNode* m_next;
        };

        void RetireBuffer(CircularBuffer<T, Allocator>* buf)
        {
            void* mem = m_allocator->Allocate(sizeof(RetiredNode), alignof(RetiredNode));
            auto* node = new (mem) RetiredNode{buf, m_retiredHead};
            m_retiredHead = node;
        }

        void FreeRetiredBuffers()
        {
            RetiredNode* node = m_retiredHead;
            while (node != nullptr)
            {
                RetiredNode* next = node->m_next;
                node->m_buffer->~CircularBuffer<T, Allocator>();
                m_allocator->Deallocate(node->m_buffer);
                m_allocator->Deallocate(node);
                node = next;
            }
            m_retiredHead = nullptr;
        }

        Allocator* m_allocator;
        std::atomic<int64_t> m_top;
        std::atomic<int64_t> m_bottom;
        std::atomic<CircularBuffer<T, Allocator>*> m_buffer;
        RetiredNode* m_retiredHead;
    };
} // namespace drone
