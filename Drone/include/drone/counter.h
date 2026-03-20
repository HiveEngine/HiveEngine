#pragma once

#include <atomic>
#include <cstdint>

namespace drone
{
    // Atomic counter with C++20 wait/notify (futex) for zero-CPU parking.
    class Counter
    {
    public:
        Counter() noexcept
            : m_value{0}
        {
        }

        explicit Counter(int64_t initial) noexcept
            : m_value{initial}
        {
        }

        void Reset(int64_t value = 0) noexcept
        {
            m_value.store(value, std::memory_order_release);
        }

        void Add(int64_t delta = 1) noexcept
        {
            m_value.fetch_add(delta, std::memory_order_release);
        }

        void Decrement() noexcept
        {
            if (m_value.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                m_value.notify_all();
            }
        }

        void Wait() const noexcept
        {
            for (;;)
            {
                int64_t val = m_value.load(std::memory_order_acquire);
                if (val <= 0)
                    return;
                m_value.wait(val, std::memory_order_relaxed);
            }
        }

        [[nodiscard]] bool IsDone() const noexcept
        {
            return m_value.load(std::memory_order_acquire) <= 0;
        }

        [[nodiscard]] int64_t Value() const noexcept
        {
            return m_value.load(std::memory_order_acquire);
        }

    private:
        std::atomic<int64_t> m_value;
    };
} // namespace drone
