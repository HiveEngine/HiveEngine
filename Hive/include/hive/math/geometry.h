#pragma once

#include <hive/math/types.h>
#include <hive/math/functions.h>

namespace hive::math
{
    struct AABB
    {
        Float3 min{};
        Float3 max{};
    };

    // ax + by + cz + d = 0, normal (a,b,c) points inward (positive halfspace = inside)
    struct Plane
    {
        float a{0.f};
        float b{0.f};
        float c{0.f};
        float d{0.f};
    };

    struct Frustum
    {
        Plane planes[6]; // left, right, bottom, top, near, far
    };

    // Gribb-Hartmann frustum extraction from a column-major view-projection matrix.
    // Mat4 convention: m[col][row], so row i element j = m[j][i].
    [[nodiscard]] inline Frustum ExtractFrustum(const Mat4& vp)
    {
        Frustum f{};
        // Left: row3 + row0
        f.planes[0] = {vp.m[0][3] + vp.m[0][0],
                        vp.m[1][3] + vp.m[1][0],
                        vp.m[2][3] + vp.m[2][0],
                        vp.m[3][3] + vp.m[3][0]};
        // Right: row3 - row0
        f.planes[1] = {vp.m[0][3] - vp.m[0][0],
                        vp.m[1][3] - vp.m[1][0],
                        vp.m[2][3] - vp.m[2][0],
                        vp.m[3][3] - vp.m[3][0]};
        // Bottom: row3 + row1
        f.planes[2] = {vp.m[0][3] + vp.m[0][1],
                        vp.m[1][3] + vp.m[1][1],
                        vp.m[2][3] + vp.m[2][1],
                        vp.m[3][3] + vp.m[3][1]};
        // Top: row3 - row1
        f.planes[3] = {vp.m[0][3] - vp.m[0][1],
                        vp.m[1][3] - vp.m[1][1],
                        vp.m[2][3] - vp.m[2][1],
                        vp.m[3][3] - vp.m[3][1]};
        // Near: row3 + row2  (for RH ZO: clip.z in [0,1])
        f.planes[4] = {vp.m[0][3] + vp.m[0][2],
                        vp.m[1][3] + vp.m[1][2],
                        vp.m[2][3] + vp.m[2][2],
                        vp.m[3][3] + vp.m[3][2]};
        // Far: row3 - row2
        f.planes[5] = {vp.m[0][3] - vp.m[0][2],
                        vp.m[1][3] - vp.m[1][2],
                        vp.m[2][3] - vp.m[2][2],
                        vp.m[3][3] - vp.m[3][2]};

        for (auto& p : f.planes)
        {
            float len = Sqrt(p.a * p.a + p.b * p.b + p.c * p.c);
            if (len > kEpsilon)
            {
                float inv = 1.f / len;
                p.a *= inv; p.b *= inv; p.c *= inv; p.d *= inv;
            }
        }
        return f;
    }

    // AABB vs frustum: returns true if the box is at least partially inside.
    // For each plane, test the "p-vertex" (corner most in the plane normal direction).
    // If the p-vertex is outside any plane, the AABB is fully outside.
    [[nodiscard]] inline bool IsVisible(const Frustum& frustum, const AABB& box)
    {
        for (int i = 0; i < 6; ++i)
        {
            const auto& p = frustum.planes[i];
            float px = (p.a >= 0.f) ? box.max.x : box.min.x;
            float py = (p.b >= 0.f) ? box.max.y : box.min.y;
            float pz = (p.c >= 0.f) ? box.max.z : box.min.z;

            if (p.a * px + p.b * py + p.c * pz + p.d < 0.f)
                return false;
        }
        return true;
    }

    // Transform an AABB by an affine 4x4 matrix (Arvo's method).
    // Returns the tightest axis-aligned box enclosing the transformed original.
    [[nodiscard]] inline AABB TransformAABB(const Mat4& m, const AABB& box)
    {
        // Start with translation (column 3, rows 0-2)
        Float3 new_min{m.m[3][0], m.m[3][1], m.m[3][2]};
        Float3 new_max = new_min;

        float src_min[3] = {box.min.x, box.min.y, box.min.z};
        float src_max[3] = {box.max.x, box.max.y, box.max.z};
        float* dst_min[3] = {&new_min.x, &new_min.y, &new_min.z};
        float* dst_max[3] = {&new_max.x, &new_max.y, &new_max.z};

        for (int col = 0; col < 3; ++col)
        {
            for (int row = 0; row < 3; ++row)
            {
                float e0 = m.m[col][row] * src_min[col];
                float e1 = m.m[col][row] * src_max[col];
                *dst_min[row] += Min(e0, e1);
                *dst_max[row] += Max(e0, e1);
            }
        }
        return {new_min, new_max};
    }
}
