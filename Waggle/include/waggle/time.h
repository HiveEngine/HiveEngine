#pragma once

#include <cstdint>

namespace waggle
{
    // Fixed-rate simulation time. Updated before each World::Advance().
    // Systems read this via queen::Res<Time>.
    struct Time
    {
        float m_dt;
        float m_elapsed;
        int64_t m_dtNs;
        int64_t m_elapsedNs;
        uint64_t m_tick;
    };

    // Per-frame render info. Updated once per rendered frame, after all fixed steps.
    // Useful for interpolation, UI, and camera smoothing.
    struct FrameInfo
    {
        float m_realDt;
        float m_realElapsed;
        int64_t m_realDtNs;
        int64_t m_realElapsedNs;
        uint64_t m_frameCount;
        float m_alpha;           // interpolation factor: accumulator_remaining / dt (0..1)
    };
} // namespace waggle
