#include <waggle/time.h>

#include <larvae/larvae.h>

namespace
{
    // Time struct

    auto test1 = larvae::RegisterTest("WaggleTime", "TimeFieldsInitialized", []() {
        waggle::Time time{
            .m_dt = 1.0f / 60.0f,
            .m_elapsed = 5.0f,
            .m_dtNs = 16'666'667,
            .m_elapsedNs = 5'000'000'000,
            .m_tick = 300,
        };

        larvae::AssertEqual(time.m_dt, 1.0f / 60.0f);
        larvae::AssertEqual(time.m_elapsed, 5.0f);
        larvae::AssertEqual(time.m_dtNs, static_cast<int64_t>(16'666'667));
        larvae::AssertEqual(time.m_elapsedNs, static_cast<int64_t>(5'000'000'000));
        larvae::AssertEqual(time.m_tick, static_cast<uint64_t>(300));
    });

    auto test2 = larvae::RegisterTest("WaggleTime", "TimeZeroInit", []() {
        waggle::Time time{};

        larvae::AssertEqual(time.m_dt, 0.0f);
        larvae::AssertEqual(time.m_elapsed, 0.0f);
        larvae::AssertEqual(time.m_dtNs, static_cast<int64_t>(0));
        larvae::AssertEqual(time.m_elapsedNs, static_cast<int64_t>(0));
        larvae::AssertEqual(time.m_tick, static_cast<uint64_t>(0));
    });

    // FrameInfo struct

    auto test3 = larvae::RegisterTest("WaggleTime", "FrameInfoFieldsInitialized", []() {
        waggle::FrameInfo info{
            .m_realDt = 0.016f,
            .m_realElapsed = 10.5f,
            .m_realDtNs = 16'000'000,
            .m_realElapsedNs = 10'500'000'000,
            .m_frameCount = 630,
            .m_alpha = 0.75f,
        };

        larvae::AssertEqual(info.m_realDt, 0.016f);
        larvae::AssertEqual(info.m_realElapsed, 10.5f);
        larvae::AssertEqual(info.m_realDtNs, static_cast<int64_t>(16'000'000));
        larvae::AssertEqual(info.m_realElapsedNs, static_cast<int64_t>(10'500'000'000));
        larvae::AssertEqual(info.m_frameCount, static_cast<uint64_t>(630));
        larvae::AssertEqual(info.m_alpha, 0.75f);
    });

    auto test4 = larvae::RegisterTest("WaggleTime", "FrameInfoZeroInit", []() {
        waggle::FrameInfo info{};

        larvae::AssertEqual(info.m_realDt, 0.0f);
        larvae::AssertEqual(info.m_realElapsed, 0.0f);
        larvae::AssertEqual(info.m_realDtNs, static_cast<int64_t>(0));
        larvae::AssertEqual(info.m_realElapsedNs, static_cast<int64_t>(0));
        larvae::AssertEqual(info.m_frameCount, static_cast<uint64_t>(0));
        larvae::AssertEqual(info.m_alpha, 0.0f);
    });

    auto test5 = larvae::RegisterTest("WaggleTime", "FrameInfoAlphaRange", []() {
        waggle::FrameInfo zero_alpha{.m_realDt = 0.0f, .m_realElapsed = 0.0f, .m_realDtNs = 0,
                                      .m_realElapsedNs = 0, .m_frameCount = 0, .m_alpha = 0.0f};
        waggle::FrameInfo full_alpha{.m_realDt = 0.0f, .m_realElapsed = 0.0f, .m_realDtNs = 0,
                                      .m_realElapsedNs = 0, .m_frameCount = 0, .m_alpha = 1.0f};

        larvae::AssertEqual(zero_alpha.m_alpha, 0.0f);
        larvae::AssertEqual(full_alpha.m_alpha, 1.0f);
    });
} // namespace
