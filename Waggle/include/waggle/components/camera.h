#pragma once

#include <hive/math/math.h>

#include <queen/reflect/component_reflector.h>
#include <queen/reflect/field_attributes.h>

namespace waggle
{

    struct Camera
    {
        float m_fovRad{hive::math::Radians(60.f)};
        float m_zNear{0.1f};
        float m_zFar{100.f};

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("fov", &Camera::m_fovRad).DisplayName("FOV").Flag(queen::FieldFlag::ANGLE);
            r.Field("z_near", &Camera::m_zNear).Range(0.001f, 10.f);
            r.Field("z_far", &Camera::m_zFar).Range(1.f, 10000.f);
        }
    };

} // namespace waggle
