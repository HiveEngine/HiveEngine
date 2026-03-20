#pragma once

#include <hive/math/math.h>

#include <queen/reflect/component_reflector.h>
#include <queen/reflect/field_attributes.h>

namespace waggle
{

    struct DirectionalLight
    {
        hive::math::Float3 m_direction{0.f, 1.f, 0.f};
        hive::math::Float3 m_color{1.f, 1.f, 1.f};
        float m_intensity{1.f};

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("direction", &DirectionalLight::m_direction);
            r.Field("color", &DirectionalLight::m_color).Flag(queen::FieldFlag::COLOR);
            r.Field("intensity", &DirectionalLight::m_intensity).Range(0.f, 10.f);
        }
    };

    struct AmbientLight
    {
        hive::math::Float3 m_color{0.1f, 0.1f, 0.1f};

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("color", &AmbientLight::m_color).Flag(queen::FieldFlag::COLOR);
        }
    };

} // namespace waggle
