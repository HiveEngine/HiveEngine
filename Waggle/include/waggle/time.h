#pragma once

#include <cstdint>

namespace waggle
{
    // Fixed-rate simulation time. Updated before each World::Advance().
    // Systems read this via queen::Res<Time>.
    struct Time
    {
        float dt;             // fixed timestep in seconds (constant, e.g. 1/60)
        float elapsed;        // total sim time in seconds
        int64_t dt_ns;        // fixed timestep in nanoseconds (authoritative)
        int64_t elapsed_ns;   // total sim time in nanoseconds (authoritative)
        uint64_t tick;        // simulation tick count (monotonic, starts at 0)
    };

    // Per-frame render info. Updated once per rendered frame, after all fixed steps.
    // Useful for interpolation, UI, and camera smoothing.
    struct FrameInfo
    {
        float real_dt;           // wall-clock time since last frame (seconds)
        float real_elapsed;      // total real wall-clock time (seconds)
        int64_t real_dt_ns;      // wall-clock time since last frame (nanoseconds)
        int64_t real_elapsed_ns; // total real wall-clock time (nanoseconds)
        uint64_t frame_count;    // rendered frame count
        float alpha;             // interpolation factor: accumulator_remaining / dt (0..1)
    };
}
