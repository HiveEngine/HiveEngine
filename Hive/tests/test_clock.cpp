#include <larvae/larvae.h>
#include <hive/core/clock.h>
#include <thread>

namespace {

    using hive::Clock;
    using hive::FrameClock;

    // =========================================================================
    // Clock basics
    // =========================================================================

    auto t_now = larvae::RegisterTest("Clock", "Now_returns_valid_time", []() {
        auto a = Clock::Now();
        auto b = Clock::Now();
        larvae::AssertTrue(Clock::NanosBetween(a, b) >= 0);
    });

    auto t_nanos_between = larvae::RegisterTest("Clock", "NanosBetween_positive", []() {
        auto start = Clock::Now();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto end = Clock::Now();
        int64_t elapsed = Clock::NanosBetween(start, end);
        larvae::AssertTrue(elapsed >= 500'000); // at least 0.5ms (sleep is imprecise)
    });

    auto t_seconds_f = larvae::RegisterTest("Clock", "SecondsF_conversion", []() {
        // 1 billion nanoseconds = 1 second
        float s = Clock::SecondsF(1'000'000'000);
        larvae::AssertTrue(std::abs(s - 1.f) < 1e-6f);
    });

    auto t_seconds_f_small = larvae::RegisterTest("Clock", "SecondsF_16ms", []() {
        // 16.666ms (one 60Hz frame)
        float s = Clock::SecondsF(16'666'667);
        larvae::AssertTrue(std::abs(s - 0.016666667f) < 1e-6f);
    });

    auto t_seconds_f_zero = larvae::RegisterTest("Clock", "SecondsF_zero", []() {
        larvae::AssertTrue(Clock::SecondsF(0) == 0.f);
    });

    // =========================================================================
    // FrameClock
    // =========================================================================

    auto t_frame_initial = larvae::RegisterTest("Clock", "FrameClock_initial_state", []() {
        FrameClock fc;
        larvae::AssertTrue(fc.delta_ns == 0);
        larvae::AssertTrue(fc.elapsed_ns == 0);
        larvae::AssertTrue(fc.frame_count == 0);
    });

    auto t_frame_tick = larvae::RegisterTest("Clock", "FrameClock_tick_advances", []() {
        FrameClock fc;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fc.Tick();
        larvae::AssertTrue(fc.frame_count == 1);
        larvae::AssertTrue(fc.delta_ns > 0);
        larvae::AssertTrue(fc.elapsed_ns > 0);
        larvae::AssertTrue(fc.elapsed_ns == fc.delta_ns);
    });

    auto t_frame_multi_tick = larvae::RegisterTest("Clock", "FrameClock_multiple_ticks", []() {
        FrameClock fc;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fc.Tick();
        int64_t first_elapsed = fc.elapsed_ns;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fc.Tick();
        larvae::AssertTrue(fc.frame_count == 2);
        larvae::AssertTrue(fc.elapsed_ns >= first_elapsed + fc.delta_ns);
    });

    auto t_frame_reset = larvae::RegisterTest("Clock", "FrameClock_reset", []() {
        FrameClock fc;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fc.Tick();
        larvae::AssertTrue(fc.frame_count > 0);

        fc.Reset();
        larvae::AssertTrue(fc.delta_ns == 0);
        larvae::AssertTrue(fc.elapsed_ns == 0);
        larvae::AssertTrue(fc.frame_count == 0);
    });

    auto t_frame_delta_seconds = larvae::RegisterTest("Clock", "FrameClock_DeltaSeconds", []() {
        FrameClock fc;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        fc.Tick();
        float dt = fc.DeltaSeconds();
        // Should be roughly 10ms (sleep is imprecise, allow wide margin)
        larvae::AssertTrue(dt >= 0.005f);
        larvae::AssertTrue(dt < 0.5f);
    });

}
