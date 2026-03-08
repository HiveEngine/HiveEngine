#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace hive
{
    // High-resolution monotonic clock.
    // steady_clock = QueryPerformanceCounter on Windows, clock_gettime(CLOCK_MONOTONIC) on Linux.
    struct Clock
    {
        using TimePoint = std::chrono::steady_clock::time_point;

        [[nodiscard]] static TimePoint Now() noexcept
        {
            return std::chrono::steady_clock::now();
        }

        [[nodiscard]] static int64_t NanosBetween(TimePoint start, TimePoint end) noexcept
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }

        // Convert nanoseconds to seconds. Safe for delta times (< 1s = no precision loss).
        // For large elapsed values (> ~4h), float precision degrades — use int64_t nanos instead.
        [[nodiscard]] static float SecondsF(int64_t nanos) noexcept
        {
            return static_cast<float>(static_cast<double>(nanos) * 1e-9);
        }
    };

    // Per-frame timing. Call Tick() once at the start of each frame.
    struct FrameClock
    {
        Clock::TimePoint m_lastTime{Clock::Now()};
        int64_t m_deltaNs{0};
        int64_t m_elapsedNs{0};
        uint64_t m_frameCount{0};

        void Reset() noexcept
        {
            m_lastTime = Clock::Now();
            m_deltaNs = 0;
            m_elapsedNs = 0;
            m_frameCount = 0;
        }

        void Tick() noexcept
        {
            auto now = Clock::Now();
            m_deltaNs = Clock::NanosBetween(m_lastTime, now);
            if (m_deltaNs < 0)
                m_deltaNs = 0; // guard against time anomalies
            m_elapsedNs += m_deltaNs;
            m_lastTime = now;
            ++m_frameCount;
        }

        [[nodiscard]] float DeltaSeconds() const noexcept
        {
            return Clock::SecondsF(m_deltaNs);
        }

        [[nodiscard]] float ElapsedSeconds() const noexcept
        {
            return Clock::SecondsF(m_elapsedNs);
        }
    };
} // namespace hive
