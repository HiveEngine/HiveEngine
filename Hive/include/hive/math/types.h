#pragma once

#include <cstdint>

namespace hive::math
{
    struct Float2
    {
        float x{0.f};
        float y{0.f};
    };

    struct Float3
    {
        float x{0.f};
        float y{0.f};
        float z{0.f};
    };

    struct Float4
    {
        float x{0.f};
        float y{0.f};
        float z{0.f};
        float w{0.f};
    };

    struct Quat
    {
        float x{0.f};
        float y{0.f};
        float z{0.f};
        float w{1.f};  // identity
    };

    // Column-major 4x4 matrix: m[col][row]
    struct Mat4
    {
        float m[4][4]{};

        [[nodiscard]] static Mat4 Identity() noexcept
        {
            Mat4 r{};
            r.m[0][0] = 1.f;
            r.m[1][1] = 1.f;
            r.m[2][2] = 1.f;
            r.m[3][3] = 1.f;
            return r;
        }
    };

    inline constexpr float kPi      = 3.14159265358979323846f;
    inline constexpr float kTwoPi   = 6.28318530717958647692f;
    inline constexpr float kHalfPi  = 1.57079632679489661923f;
    inline constexpr float kEpsilon = 1.0e-6f;

    [[nodiscard]] inline constexpr float Radians(float degrees) { return degrees * (kPi / 180.f); }
    [[nodiscard]] inline constexpr float Degrees(float radians) { return radians * (180.f / kPi); }
}
