#pragma once

#include <hive/math/math.h>

namespace waggle
{

struct CameraView
{
    hive::math::Mat4 view{};
    hive::math::Mat4 projection{};
    hive::math::Float3 position{};
    float z_near{0.1f};
    float z_far{100.f};
    float fov_rad{hive::math::Radians(60.f)};
    float aspect{16.f / 9.f};
};

} // namespace waggle
