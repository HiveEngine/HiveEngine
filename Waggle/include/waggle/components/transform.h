#pragma once

#include <hive/math/math.h>
#include <queen/reflect/component_reflector.h>
#include <cstdint>

namespace waggle
{

struct Transform
{
    hive::math::Float3 position{0.f, 0.f, 0.f};
    hive::math::Quat   rotation{};
    hive::math::Float3 scale{1.f, 1.f, 1.f};

    static void Reflect(queen::ComponentReflector<>& r)
    {
        r.Field("position", &Transform::position);
        r.Field("rotation", &Transform::rotation);
        r.Field("scale", &Transform::scale);
    }
};

struct WorldMatrix
{
    hive::math::Mat4 matrix{hive::math::Mat4::Identity()};
};

struct TransformVersion
{
    uint32_t last_modified{0};
};

struct LocalAABB
{
    hive::math::AABB bounds{};
};

struct WorldAABB
{
    hive::math::AABB bounds{};
};

} // namespace waggle
