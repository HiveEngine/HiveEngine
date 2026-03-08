#pragma once

#include <hive/math/functions.h>
#include <hive/math/types.h>

namespace hive::math
{
    struct AABB
    {
        Float3 m_min{};
        Float3 m_max{};
    };

    struct Plane
    {
        float m_a{0.f};
        float m_b{0.f};
        float m_c{0.f};
        float m_d{0.f};
    };

    struct Frustum
    {
        Plane m_planes[6];
    };

    [[nodiscard]] inline Frustum ExtractFrustum(const Mat4& vp)
    {
        Frustum frustum{};
        frustum.m_planes[0] = {vp.m_m[0][3] + vp.m_m[0][0], vp.m_m[1][3] + vp.m_m[1][0], vp.m_m[2][3] + vp.m_m[2][0],
                               vp.m_m[3][3] + vp.m_m[3][0]};
        frustum.m_planes[1] = {vp.m_m[0][3] - vp.m_m[0][0], vp.m_m[1][3] - vp.m_m[1][0], vp.m_m[2][3] - vp.m_m[2][0],
                               vp.m_m[3][3] - vp.m_m[3][0]};
        frustum.m_planes[2] = {vp.m_m[0][3] + vp.m_m[0][1], vp.m_m[1][3] + vp.m_m[1][1], vp.m_m[2][3] + vp.m_m[2][1],
                               vp.m_m[3][3] + vp.m_m[3][1]};
        frustum.m_planes[3] = {vp.m_m[0][3] - vp.m_m[0][1], vp.m_m[1][3] - vp.m_m[1][1], vp.m_m[2][3] - vp.m_m[2][1],
                               vp.m_m[3][3] - vp.m_m[3][1]};
        frustum.m_planes[4] = {vp.m_m[0][3] + vp.m_m[0][2], vp.m_m[1][3] + vp.m_m[1][2], vp.m_m[2][3] + vp.m_m[2][2],
                               vp.m_m[3][3] + vp.m_m[3][2]};
        frustum.m_planes[5] = {vp.m_m[0][3] - vp.m_m[0][2], vp.m_m[1][3] - vp.m_m[1][2], vp.m_m[2][3] - vp.m_m[2][2],
                               vp.m_m[3][3] - vp.m_m[3][2]};

        for (Plane& plane : frustum.m_planes)
        {
            const float length = Sqrt(plane.m_a * plane.m_a + plane.m_b * plane.m_b + plane.m_c * plane.m_c);
            if (length > kEpsilon)
            {
                const float invLength = 1.f / length;
                plane.m_a *= invLength;
                plane.m_b *= invLength;
                plane.m_c *= invLength;
                plane.m_d *= invLength;
            }
        }

        return frustum;
    }

    [[nodiscard]] inline bool IsVisible(const Frustum& frustum, const AABB& box)
    {
        for (const Plane& plane : frustum.m_planes)
        {
            const float px = plane.m_a >= 0.f ? box.m_max.m_x : box.m_min.m_x;
            const float py = plane.m_b >= 0.f ? box.m_max.m_y : box.m_min.m_y;
            const float pz = plane.m_c >= 0.f ? box.m_max.m_z : box.m_min.m_z;

            if (plane.m_a * px + plane.m_b * py + plane.m_c * pz + plane.m_d < 0.f)
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] inline AABB TransformAABB(const Mat4& m, const AABB& box)
    {
        Float3 newMin{m.m_m[3][0], m.m_m[3][1], m.m_m[3][2]};
        Float3 newMax = newMin;

        const float srcMin[3] = {box.m_min.m_x, box.m_min.m_y, box.m_min.m_z};
        const float srcMax[3] = {box.m_max.m_x, box.m_max.m_y, box.m_max.m_z};
        float* dstMin[3] = {&newMin.m_x, &newMin.m_y, &newMin.m_z};
        float* dstMax[3] = {&newMax.m_x, &newMax.m_y, &newMax.m_z};

        for (int column = 0; column < 3; ++column)
        {
            for (int row = 0; row < 3; ++row)
            {
                const float e0 = m.m_m[column][row] * srcMin[column];
                const float e1 = m.m_m[column][row] * srcMax[column];
                *dstMin[row] += Min(e0, e1);
                *dstMax[row] += Max(e0, e1);
            }
        }

        return {newMin, newMax};
    }
} // namespace hive::math
