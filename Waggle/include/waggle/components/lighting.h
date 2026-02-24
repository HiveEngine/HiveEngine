#pragma once

#include <hive/math/math.h>
#include <queen/reflect/component_reflector.h>
#include <queen/reflect/field_attributes.h>

namespace waggle
{

struct DirectionalLight
{
    hive::math::Float3 direction{0.f, 1.f, 0.f};
    hive::math::Float3 color{1.f, 1.f, 1.f};
    float intensity{1.f};

    static void Reflect(queen::ComponentReflector<>& r)
    {
        r.Field("direction", &DirectionalLight::direction);
        r.Field("color", &DirectionalLight::color).Flag(queen::FieldFlag::Color);
        r.Field("intensity", &DirectionalLight::intensity).Range(0.f, 10.f);
    }
};

struct AmbientLight
{
    hive::math::Float3 color{0.1f, 0.1f, 0.1f};

    static void Reflect(queen::ComponentReflector<>& r)
    {
        r.Field("color", &AmbientLight::color).Flag(queen::FieldFlag::Color);
    }
};

} // namespace waggle
