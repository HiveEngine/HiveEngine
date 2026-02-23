#pragma once

#include <hive/math/math.h>
#include <queen/reflect/component_reflector.h>
#include <queen/reflect/field_attributes.h>

namespace waggle
{

struct Camera
{
    float fov_rad{hive::math::Radians(60.f)};
    float z_near{0.1f};
    float z_far{100.f};

    static void Reflect(queen::ComponentReflector<>& r)
    {
        r.Field("fov", &Camera::fov_rad).DisplayName("FOV").Flag(queen::FieldFlag::Angle);
        r.Field("z_near", &Camera::z_near).Range(0.001f, 10.f);
        r.Field("z_far", &Camera::z_far).Range(1.f, 10000.f);
    }
};

} // namespace waggle
