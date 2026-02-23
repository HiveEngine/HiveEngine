#include <larvae/larvae.h>
#include <hive/math/math.h>
#include <cmath>

namespace {

    using namespace hive::math;

    constexpr float kTol = 1e-5f;

    bool Near(float a, float b) { return std::fabs(a - b) < kTol; }

    // =========================================================================
    // Float3 basics
    // =========================================================================

    auto t_f3_add = larvae::RegisterTest("Math", "Float3_Add", []() {
        Float3 a{1.f, 2.f, 3.f};
        Float3 b{4.f, 5.f, 6.f};
        Float3 r = a + b;
        larvae::AssertTrue(Near(r.x, 5.f));
        larvae::AssertTrue(Near(r.y, 7.f));
        larvae::AssertTrue(Near(r.z, 9.f));
    });

    auto t_f3_sub = larvae::RegisterTest("Math", "Float3_Sub", []() {
        Float3 a{4.f, 5.f, 6.f};
        Float3 b{1.f, 2.f, 3.f};
        Float3 r = a - b;
        larvae::AssertTrue(Near(r.x, 3.f));
        larvae::AssertTrue(Near(r.y, 3.f));
        larvae::AssertTrue(Near(r.z, 3.f));
    });

    auto t_f3_scale = larvae::RegisterTest("Math", "Float3_Scale", []() {
        Float3 a{1.f, 2.f, 3.f};
        Float3 r = a * 2.f;
        larvae::AssertTrue(Near(r.x, 2.f));
        larvae::AssertTrue(Near(r.y, 4.f));
        larvae::AssertTrue(Near(r.z, 6.f));
    });

    auto t_f3_negate = larvae::RegisterTest("Math", "Float3_Negate", []() {
        Float3 a{1.f, -2.f, 3.f};
        Float3 r = -a;
        larvae::AssertTrue(Near(r.x, -1.f));
        larvae::AssertTrue(Near(r.y, 2.f));
        larvae::AssertTrue(Near(r.z, -3.f));
    });

    auto t_f3_dot = larvae::RegisterTest("Math", "Float3_Dot", []() {
        Float3 a{1.f, 0.f, 0.f};
        Float3 b{0.f, 1.f, 0.f};
        larvae::AssertTrue(Near(Dot(a, b), 0.f));

        Float3 c{1.f, 2.f, 3.f};
        Float3 d{4.f, 5.f, 6.f};
        larvae::AssertTrue(Near(Dot(c, d), 32.f));
    });

    auto t_f3_cross = larvae::RegisterTest("Math", "Float3_Cross", []() {
        Float3 x{1.f, 0.f, 0.f};
        Float3 y{0.f, 1.f, 0.f};
        Float3 z = Cross(x, y);
        larvae::AssertTrue(Near(z.x, 0.f));
        larvae::AssertTrue(Near(z.y, 0.f));
        larvae::AssertTrue(Near(z.z, 1.f));
    });

    auto t_f3_length = larvae::RegisterTest("Math", "Float3_Length", []() {
        Float3 a{3.f, 4.f, 0.f};
        larvae::AssertTrue(Near(Length(a), 5.f));
        larvae::AssertTrue(Near(LengthSq(a), 25.f));
    });

    auto t_f3_normalize = larvae::RegisterTest("Math", "Float3_Normalize", []() {
        Float3 a{0.f, 3.f, 4.f};
        Float3 n = Normalize(a);
        larvae::AssertTrue(Near(Length(n), 1.f));
        larvae::AssertTrue(Near(n.y, 3.f / 5.f));
        larvae::AssertTrue(Near(n.z, 4.f / 5.f));
    });

    auto t_f3_lerp = larvae::RegisterTest("Math", "Float3_Lerp", []() {
        Float3 a{0.f, 0.f, 0.f};
        Float3 b{10.f, 20.f, 30.f};
        Float3 r = Lerp(a, 0.5f, b);
        larvae::AssertTrue(Near(r.x, 5.f));
        larvae::AssertTrue(Near(r.y, 10.f));
        larvae::AssertTrue(Near(r.z, 15.f));
    });

    // =========================================================================
    // Mat4 basics
    // =========================================================================

    auto t_m4_identity = larvae::RegisterTest("Math", "Mat4_Identity", []() {
        Mat4 I = Mat4::Identity();
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                larvae::AssertTrue(Near(I.m[c][r], c == r ? 1.f : 0.f));
    });

    auto t_m4_mul_identity = larvae::RegisterTest("Math", "Mat4_MulIdentity", []() {
        Mat4 I = Mat4::Identity();
        Mat4 A = I;
        A.m[3][0] = 5.f; // translation x
        A.m[3][1] = 3.f; // translation y

        Mat4 R = A * I;
        larvae::AssertTrue(Near(R.m[3][0], 5.f));
        larvae::AssertTrue(Near(R.m[3][1], 3.f));
    });

    auto t_m4_transform_point = larvae::RegisterTest("Math", "Mat4_TransformPoint", []() {
        Mat4 T = Translation({2.f, 3.f, 4.f});
        Float4 p{1.f, 1.f, 1.f, 1.f};
        Float4 r = T * p;
        larvae::AssertTrue(Near(r.x, 3.f));
        larvae::AssertTrue(Near(r.y, 4.f));
        larvae::AssertTrue(Near(r.z, 5.f));
        larvae::AssertTrue(Near(r.w, 1.f));
    });

    auto t_m4_transpose = larvae::RegisterTest("Math", "Mat4_Transpose", []() {
        Mat4 m{};
        m.m[0][1] = 2.f;  // col 0, row 1
        m.m[1][0] = 3.f;  // col 1, row 0
        Mat4 t = Transpose(m);
        larvae::AssertTrue(Near(t.m[1][0], 2.f));
        larvae::AssertTrue(Near(t.m[0][1], 3.f));
    });

    auto t_m4_inverse = larvae::RegisterTest("Math", "Mat4_Inverse", []() {
        Mat4 T = Translation({5.f, 10.f, 15.f});
        Mat4 Tinv = Inverse(T);
        Mat4 I = T * Tinv;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                larvae::AssertTrue(Near(I.m[c][r], c == r ? 1.f : 0.f));
    });

    // =========================================================================
    // Transforms
    // =========================================================================

    auto t_translation = larvae::RegisterTest("Math", "Translation", []() {
        Mat4 T = Translation({1.f, 2.f, 3.f});
        // Column-major: translation in column 3
        larvae::AssertTrue(Near(T.m[3][0], 1.f));
        larvae::AssertTrue(Near(T.m[3][1], 2.f));
        larvae::AssertTrue(Near(T.m[3][2], 3.f));
        // Diagonal = 1
        larvae::AssertTrue(Near(T.m[0][0], 1.f));
        larvae::AssertTrue(Near(T.m[1][1], 1.f));
        larvae::AssertTrue(Near(T.m[2][2], 1.f));
        larvae::AssertTrue(Near(T.m[3][3], 1.f));
    });

    auto t_scale = larvae::RegisterTest("Math", "Scale", []() {
        Mat4 S = Scale({2.f, 3.f, 4.f});
        Float4 p{1.f, 1.f, 1.f, 1.f};
        Float4 r = S * p;
        larvae::AssertTrue(Near(r.x, 2.f));
        larvae::AssertTrue(Near(r.y, 3.f));
        larvae::AssertTrue(Near(r.z, 4.f));
    });

    auto t_perspective = larvae::RegisterTest("Math", "Perspective", []() {
        Mat4 P = Perspective(Radians(90.f), 1.f, 0.1f, 100.f);
        // FOV 90° → cot(45°) = 1.0, negated for Vulkan Y-flip
        larvae::AssertTrue(Near(P.m[0][0], 1.f));
        larvae::AssertTrue(Near(P.m[1][1], -1.f));
        // Vulkan RH_ZO: m[2][3] = -1
        larvae::AssertTrue(Near(P.m[2][3], -1.f));
    });

    auto t_lookat = larvae::RegisterTest("Math", "LookAt", []() {
        Mat4 V = LookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
        // Camera at (0,0,5) looking at origin: should translate -5 on Z
        Float4 origin{0.f, 0.f, 0.f, 1.f};
        Float4 r = V * origin;
        // In view space, origin should be at (0, 0, -5) for RH
        larvae::AssertTrue(Near(r.x, 0.f));
        larvae::AssertTrue(Near(r.y, 0.f));
        larvae::AssertTrue(Near(r.z, -5.f));
    });

    auto t_trs = larvae::RegisterTest("Math", "TRS", []() {
        Float3 pos{1.f, 2.f, 3.f};
        Quat rot{0.f, 0.f, 0.f, 1.f}; // identity
        Float3 scl{2.f, 2.f, 2.f};

        Mat4 M = TRS(pos, rot, scl);
        Float4 p{1.f, 0.f, 0.f, 1.f};
        Float4 r = M * p;
        // Scale by 2 → (2,0,0), then translate → (3,2,3)
        larvae::AssertTrue(Near(r.x, 3.f));
        larvae::AssertTrue(Near(r.y, 2.f));
        larvae::AssertTrue(Near(r.z, 3.f));
    });

    // =========================================================================
    // Quaternion
    // =========================================================================

    auto t_quat_identity = larvae::RegisterTest("Math", "Quat_Identity", []() {
        Quat q{0.f, 0.f, 0.f, 1.f};
        Mat4 M = QuatToMat4(q);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                larvae::AssertTrue(Near(M.m[c][r], c == r ? 1.f : 0.f));
    });

    auto t_quat_rotation_90 = larvae::RegisterTest("Math", "Quat_Rotation90Y", []() {
        // 90° rotation around Y axis
        Quat q = QuatFromAxisAngle({0.f, 1.f, 0.f}, Radians(90.f));
        Float3 v{1.f, 0.f, 0.f};
        Float3 r = RotateByQuat(v, q);
        // RH: rotating X around Y by 90° → -Z
        larvae::AssertTrue(Near(r.x, 0.f));
        larvae::AssertTrue(Near(r.y, 0.f));
        larvae::AssertTrue(Near(r.z, -1.f));
    });

    auto t_quat_normalize = larvae::RegisterTest("Math", "Quat_Normalize", []() {
        Quat q{1.f, 2.f, 3.f, 4.f};
        Quat n = Normalize(q);
        float len = Sqrt(Dot(n, n));
        larvae::AssertTrue(Near(len, 1.f));
    });

    auto t_quat_slerp = larvae::RegisterTest("Math", "Quat_Slerp", []() {
        Quat a{0.f, 0.f, 0.f, 1.f};
        Quat b = QuatFromAxisAngle({0.f, 1.f, 0.f}, Radians(90.f));
        Quat half = SLerp(a, 0.5f, b);
        // Should be ~45° rotation
        Float3 v{1.f, 0.f, 0.f};
        Float3 r = RotateByQuat(v, half);
        // cos(45°) ≈ 0.7071
        larvae::AssertTrue(Near(r.x, 0.7071f) || Near(r.x, std::cos(Radians(45.f))));
    });

    // =========================================================================
    // Float2
    // =========================================================================

    auto t_f2_ops = larvae::RegisterTest("Math", "Float2_Ops", []() {
        Float2 a{1.f, 2.f};
        Float2 b{3.f, 4.f};
        Float2 r = a + b;
        larvae::AssertTrue(Near(r.x, 4.f));
        larvae::AssertTrue(Near(r.y, 6.f));

        larvae::AssertTrue(Near(Dot(a, b), 11.f));
        larvae::AssertTrue(Near(Length(Float2{3.f, 4.f}), 5.f));
    });

    // =========================================================================
    // Float4
    // =========================================================================

    auto t_f4_ops = larvae::RegisterTest("Math", "Float4_Ops", []() {
        Float4 a{1.f, 2.f, 3.f, 4.f};
        Float4 b{5.f, 6.f, 7.f, 8.f};
        Float4 r = a + b;
        larvae::AssertTrue(Near(r.x, 6.f));
        larvae::AssertTrue(Near(r.w, 12.f));

        larvae::AssertTrue(Near(Dot(a, b), 70.f));
    });

    // =========================================================================
    // Determinism: same operations produce identical results
    // =========================================================================

    auto t_determinism = larvae::RegisterTest("Math", "Determinism", []() {
        // Run the same computation twice, verify bitwise identical results
        auto compute = []() {
            Float3 v{1.23456f, 7.89012f, 3.45678f};
            Mat4 M = Perspective(Radians(60.f), 16.f / 9.f, 0.1f, 1000.f);
            Mat4 V = LookAt({10.f, 5.f, 10.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
            Mat4 VP = M * V;
            Float4 result = VP * Float4{v.x, v.y, v.z, 1.f};
            return result;
        };

        Float4 r1 = compute();
        Float4 r2 = compute();

        // Bitwise identical (not just near — deterministic)
        larvae::AssertEqual(r1.x, r2.x);
        larvae::AssertEqual(r1.y, r2.y);
        larvae::AssertEqual(r1.z, r2.z);
        larvae::AssertEqual(r1.w, r2.w);
    });

    // =========================================================================
    // Constants
    // =========================================================================

    auto t_constants = larvae::RegisterTest("Math", "Constants", []() {
        larvae::AssertTrue(Near(Radians(180.f), kPi));
        larvae::AssertTrue(Near(Degrees(kPi), 180.f));
        larvae::AssertTrue(Near(Radians(90.f), kHalfPi));
    });

} // namespace
