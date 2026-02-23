#include <larvae/larvae.h>
#include <hive/math/math.h>
#include <cmath>

namespace {

    using namespace hive::math;

    constexpr float kTol = 1e-4f;

    bool Near(float a, float b) { return std::fabs(a - b) < kTol; }

    // =========================================================================
    // AABB
    // =========================================================================

    auto t_aabb_default = larvae::RegisterTest("Geometry", "AABB_Default", []() {
        AABB box{};
        larvae::AssertTrue(Near(box.min.x, 0.f));
        larvae::AssertTrue(Near(box.max.x, 0.f));
    });

    auto t_aabb_construct = larvae::RegisterTest("Geometry", "AABB_Construction", []() {
        AABB box{{-1.f, -2.f, -3.f}, {4.f, 5.f, 6.f}};
        larvae::AssertTrue(Near(box.min.x, -1.f));
        larvae::AssertTrue(Near(box.min.y, -2.f));
        larvae::AssertTrue(Near(box.max.z, 6.f));
    });

    // =========================================================================
    // TransformAABB
    // =========================================================================

    auto t_xform_identity = larvae::RegisterTest("Geometry", "TransformAABB_Identity", []() {
        AABB box{{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}};
        AABB result = TransformAABB(Mat4::Identity(), box);
        larvae::AssertTrue(Near(result.min.x, -1.f));
        larvae::AssertTrue(Near(result.min.y, -1.f));
        larvae::AssertTrue(Near(result.min.z, -1.f));
        larvae::AssertTrue(Near(result.max.x, 1.f));
        larvae::AssertTrue(Near(result.max.y, 1.f));
        larvae::AssertTrue(Near(result.max.z, 1.f));
    });

    auto t_xform_translate = larvae::RegisterTest("Geometry", "TransformAABB_Translation", []() {
        AABB box{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};
        Mat4 m = Translation({10.f, 20.f, 30.f});
        AABB result = TransformAABB(m, box);
        larvae::AssertTrue(Near(result.min.x, 10.f));
        larvae::AssertTrue(Near(result.min.y, 20.f));
        larvae::AssertTrue(Near(result.min.z, 30.f));
        larvae::AssertTrue(Near(result.max.x, 11.f));
        larvae::AssertTrue(Near(result.max.y, 21.f));
        larvae::AssertTrue(Near(result.max.z, 31.f));
    });

    auto t_xform_scale = larvae::RegisterTest("Geometry", "TransformAABB_Scale", []() {
        AABB box{{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}};
        Mat4 m = Scale({2.f, 3.f, 4.f});
        AABB result = TransformAABB(m, box);
        larvae::AssertTrue(Near(result.min.x, -2.f));
        larvae::AssertTrue(Near(result.min.y, -3.f));
        larvae::AssertTrue(Near(result.min.z, -4.f));
        larvae::AssertTrue(Near(result.max.x, 2.f));
        larvae::AssertTrue(Near(result.max.y, 3.f));
        larvae::AssertTrue(Near(result.max.z, 4.f));
    });

    auto t_xform_rotation = larvae::RegisterTest("Geometry", "TransformAABB_Rotation45Y", []() {
        // Unit cube [0,1]^3 rotated 45 degrees around Y (RH)
        // Arvo method gives: min=(0, 0, -0.707), max=(1.414, 1, 0.707)
        AABB box{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};
        Quat rot = QuatFromAxisAngle({0.f, 1.f, 0.f}, Radians(45.f));
        Mat4 m = Rotation(rot);
        AABB result = TransformAABB(m, box);

        float c = Cos(Radians(45.f)); // ~0.707
        larvae::AssertTrue(Near(result.min.x, 0.f));
        larvae::AssertTrue(Near(result.max.x, c + c)); // ~1.414
        larvae::AssertTrue(Near(result.min.y, 0.f));
        larvae::AssertTrue(Near(result.max.y, 1.f));
        larvae::AssertTrue(Near(result.min.z, -c));     // ~-0.707
        larvae::AssertTrue(Near(result.max.z, c));       // ~0.707
    });

    auto t_xform_trs = larvae::RegisterTest("Geometry", "TransformAABB_TRS", []() {
        AABB box{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};
        Mat4 m = TRS({5.f, 0.f, 0.f}, Quat{}, {2.f, 2.f, 2.f});
        AABB result = TransformAABB(m, box);
        larvae::AssertTrue(Near(result.min.x, 5.f));
        larvae::AssertTrue(Near(result.max.x, 7.f)); // 5 + 2*1
        larvae::AssertTrue(Near(result.min.y, 0.f));
        larvae::AssertTrue(Near(result.max.y, 2.f));
    });

    // =========================================================================
    // Frustum extraction + visibility
    // =========================================================================

    // Helper: build a known view_proj from LookAt + Perspective
    Frustum MakeTestFrustum()
    {
        // Camera at origin looking down -Z
        Mat4 view = LookAt({0.f, 0.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 1.f, 0.f});
        Mat4 proj = Perspective(Radians(90.f), 1.f, 0.1f, 100.f);
        return ExtractFrustum(proj * view);
    }

    auto t_frustum_extract = larvae::RegisterTest("Geometry", "ExtractFrustum_HasNormalizedPlanes", []() {
        Frustum f = MakeTestFrustum();
        for (int i = 0; i < 6; ++i)
        {
            float len = Sqrt(f.planes[i].a * f.planes[i].a +
                             f.planes[i].b * f.planes[i].b +
                             f.planes[i].c * f.planes[i].c);
            larvae::AssertTrue(Near(len, 1.f));
        }
    });

    auto t_visible_inside = larvae::RegisterTest("Geometry", "IsVisible_Inside", []() {
        Frustum f = MakeTestFrustum();
        // Box at z=-5 (in front of camera), well within frustum
        AABB box{{-1.f, -1.f, -6.f}, {1.f, 1.f, -4.f}};
        larvae::AssertTrue(IsVisible(f, box));
    });

    auto t_visible_behind = larvae::RegisterTest("Geometry", "IsVisible_Behind", []() {
        Frustum f = MakeTestFrustum();
        // Box behind camera (z > 0)
        AABB box{{-1.f, -1.f, 5.f}, {1.f, 1.f, 10.f}};
        larvae::AssertTrue(!IsVisible(f, box));
    });

    auto t_visible_far = larvae::RegisterTest("Geometry", "IsVisible_BeyondFar", []() {
        Frustum f = MakeTestFrustum();
        // Box past far plane (z < -100)
        AABB box{{-1.f, -1.f, -200.f}, {1.f, 1.f, -150.f}};
        larvae::AssertTrue(!IsVisible(f, box));
    });

    auto t_visible_left = larvae::RegisterTest("Geometry", "IsVisible_OutsideLeft", []() {
        Frustum f = MakeTestFrustum();
        // Box far to the left at z=-10. FOV=90 means frustum half-width = z at that depth.
        // At z=-10, visible range is about [-10, 10] in X.
        AABB box{{-50.f, -1.f, -11.f}, {-40.f, 1.f, -9.f}};
        larvae::AssertTrue(!IsVisible(f, box));
    });

    auto t_visible_right = larvae::RegisterTest("Geometry", "IsVisible_OutsideRight", []() {
        Frustum f = MakeTestFrustum();
        AABB box{{40.f, -1.f, -11.f}, {50.f, 1.f, -9.f}};
        larvae::AssertTrue(!IsVisible(f, box));
    });

    auto t_visible_above = larvae::RegisterTest("Geometry", "IsVisible_OutsideAbove", []() {
        Frustum f = MakeTestFrustum();
        AABB box{{-1.f, 40.f, -11.f}, {1.f, 50.f, -9.f}};
        larvae::AssertTrue(!IsVisible(f, box));
    });

    auto t_visible_below = larvae::RegisterTest("Geometry", "IsVisible_OutsideBelow", []() {
        Frustum f = MakeTestFrustum();
        AABB box{{-1.f, -50.f, -11.f}, {1.f, -40.f, -9.f}};
        larvae::AssertTrue(!IsVisible(f, box));
    });

    auto t_visible_straddling = larvae::RegisterTest("Geometry", "IsVisible_Straddling", []() {
        Frustum f = MakeTestFrustum();
        // Box straddles the near plane
        AABB box{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
        larvae::AssertTrue(IsVisible(f, box));
    });

    auto t_visible_at_near = larvae::RegisterTest("Geometry", "IsVisible_AtNearPlane", []() {
        Frustum f = MakeTestFrustum();
        // Small box just past the near plane
        AABB box{{-0.01f, -0.01f, -0.15f}, {0.01f, 0.01f, -0.11f}};
        larvae::AssertTrue(IsVisible(f, box));
    });

} // namespace
