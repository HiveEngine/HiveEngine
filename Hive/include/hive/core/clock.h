#pragma once

#include <chrono>
#include <cstdint>
#include <algorithm>

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
        Clock::TimePoint last_time{Clock::Now()};
        int64_t  delta_ns{0};
        int64_t  elapsed_ns{0};
        uint64_t frame_count{0};

        void Reset() noexcept
        {
            last_time = Clock::Now();
            delta_ns = 0;
            elapsed_ns = 0;
            frame_count = 0;
        }

        void Tick() noexcept
        {
            auto now = Clock::Now();
            delta_ns = Clock::NanosBetween(last_time, now);
            if (delta_ns < 0) delta_ns = 0; // guard against time anomalies
            elapsed_ns += delta_ns;
            last_time = now;
            ++frame_count;
        }

        [[nodiscard]] float DeltaSeconds() const noexcept
        {
            return Clock::SecondsF(delta_ns);
        }

        [[nodiscard]] float ElapsedSeconds() const noexcept
        {
            return Clock::SecondsF(elapsed_ns);
        }
    };
}
