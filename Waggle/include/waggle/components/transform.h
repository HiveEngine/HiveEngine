#pragma once

#include <hive/math/math.h>

#include <queen/reflect/component_reflector.h>

#include <cstdint>

namespace waggle
{

    struct Transform
    {
        hive::math::Float3 m_position{0.f, 0.f, 0.f};
        hive::math::Quat m_rotation{};
        hive::math::Float3 m_scale{1.f, 1.f, 1.f};

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("position", &Transform::m_position);
            r.Field("rotation", &Transform::m_rotation);
            r.Field("scale", &Transform::m_scale);
        }
    };

    struct WorldMatrix
    {
        hive::math::Mat4 m_matrix{hive::math::Mat4::Identity()};
    };

    struct TransformVersion
    {
        uint64_t m_lastModified{0};
    };

    struct LocalAABB
    {
        hive::math::AABB m_bounds{};
    };

    struct WorldAABB
    {
        hive::math::AABB m_bounds{};
    };

} // namespace waggle
