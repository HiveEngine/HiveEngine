#pragma once

#include <cstdint>

namespace waggle
{
    // Fixed-rate simulation time. Updated before each World::Advance().
    // Systems read this via queen::Res<Time>.
    struct Time
    {
        float m_dt;          // fixed timestep in seconds (constant, e.g. 1/60)
        float m_elapsed;     // total sim time in seconds
        int64_t m_dtNs;      // fixed timestep in nanoseconds (authoritative)
        int64_t m_elapsedNs; // total sim time in nanoseconds (authoritative)
        uint64_t m_tick;     // simulation tick count (monotonic, starts at 0)
    };

    // Per-frame render info. Updated once per rendered frame, after all fixed steps.
    // Useful for interpolation, UI, and camera smoothing.
    struct FrameInfo
    {
        float m_realDt;          // wall-clock time since last frame (seconds)
        float m_realElapsed;     // total real wall-clock time (seconds)
        int64_t m_realDtNs;      // wall-clock time since last frame (nanoseconds)
        int64_t m_realElapsedNs; // total real wall-clock time (nanoseconds)
        uint64_t m_frameCount;   // rendered frame count
        float m_alpha;           // interpolation factor: accumulator_remaining / dt (0..1)
    };
} // namespace waggle
