#include <hive/math/math.h>

#include <larvae/larvae.h>

#include <cmath>

namespace
{

    using namespace hive::math;

    constexpr float kTol = 1e-5f;

    bool Near(float a, float b)
    {
        return std::fabs(a - b) < kTol;
    }

    // =========================================================================
    // Float3 basics
    // =========================================================================

    auto t_f3_add = larvae::RegisterTest("Math", "Float3_Add", []() {
        Float3 a{1.f, 2.f, 3.f};
        Float3 b{4.f, 5.f, 6.f};
        Float3 r = a + b;
        larvae::AssertTrue(Near(r.m_x, 5.f));
        larvae::AssertTrue(Near(r.m_y, 7.f));
        larvae::AssertTrue(Near(r.m_z, 9.f));
    });

    auto t_f3_sub = larvae::RegisterTest("Math", "Float3_Sub", []() {
        Float3 a{4.f, 5.f, 6.f};
        Float3 b{1.f, 2.f, 3.f};
        Float3 r = a - b;
        larvae::AssertTrue(Near(r.m_x, 3.f));
        larvae::AssertTrue(Near(r.m_y, 3.f));
        larvae::AssertTrue(Near(r.m_z, 3.f));
    });

    auto t_f3_scale = larvae::RegisterTest("Math", "Float3_Scale", []() {
        Float3 a{1.f, 2.f, 3.f};
        Float3 r = a * 2.f;
        larvae::AssertTrue(Near(r.m_x, 2.f));
        larvae::AssertTrue(Near(r.m_y, 4.f));
        larvae::AssertTrue(Near(r.m_z, 6.f));
    });

    auto t_f3_negate = larvae::RegisterTest("Math", "Float3_Negate", []() {
        Float3 a{1.f, -2.f, 3.f};
        Float3 r = -a;
        larvae::AssertTrue(Near(r.m_x, -1.f));
        larvae::AssertTrue(Near(r.m_y, 2.f));
        larvae::AssertTrue(Near(r.m_z, -3.f));
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
        larvae::AssertTrue(Near(z.m_x, 0.f));
        larvae::AssertTrue(Near(z.m_y, 0.f));
        larvae::AssertTrue(Near(z.m_z, 1.f));
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
        larvae::AssertTrue(Near(n.m_y, 3.f / 5.f));
        larvae::AssertTrue(Near(n.m_z, 4.f / 5.f));
    });

    auto t_f3_lerp = larvae::RegisterTest("Math", "Float3_Lerp", []() {
        Float3 a{0.f, 0.f, 0.f};
        Float3 b{10.f, 20.f, 30.f};
        Float3 r = Lerp(a, 0.5f, b);
        larvae::AssertTrue(Near(r.m_x, 5.f));
        larvae::AssertTrue(Near(r.m_y, 10.f));
        larvae::AssertTrue(Near(r.m_z, 15.f));
    });

    // =========================================================================
    // Mat4 basics
    // =========================================================================

    auto t_m4_identity = larvae::RegisterTest("Math", "Mat4_Identity", []() {
        Mat4 identity = Mat4::Identity();
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                larvae::AssertTrue(Near(identity.m_m[col][row], col == row ? 1.f : 0.f));
    });

    auto t_m4_mul_identity = larvae::RegisterTest("Math", "Mat4_MulIdentity", []() {
        Mat4 identity = Mat4::Identity();
        Mat4 a = identity;
        a.m_m[3][0] = 5.f;
        a.m_m[3][1] = 3.f;

        Mat4 result = a * identity;
        larvae::AssertTrue(Near(result.m_m[3][0], 5.f));
        larvae::AssertTrue(Near(result.m_m[3][1], 3.f));
    });

    auto t_m4_transform_point = larvae::RegisterTest("Math", "Mat4_TransformPoint", []() {
        Mat4 transform = Translation({2.f, 3.f, 4.f});
        Float4 point{1.f, 1.f, 1.f, 1.f};
        Float4 result = transform * point;
        larvae::AssertTrue(Near(result.m_x, 3.f));
        larvae::AssertTrue(Near(result.m_y, 4.f));
        larvae::AssertTrue(Near(result.m_z, 5.f));
        larvae::AssertTrue(Near(result.m_w, 1.f));
    });

    auto t_m4_transpose = larvae::RegisterTest("Math", "Mat4_Transpose", []() {
        Mat4 matrix{};
        matrix.m_m[0][1] = 2.f;
        matrix.m_m[1][0] = 3.f;
        Mat4 transposed = Transpose(matrix);
        larvae::AssertTrue(Near(transposed.m_m[1][0], 2.f));
        larvae::AssertTrue(Near(transposed.m_m[0][1], 3.f));
    });

    auto t_m4_inverse = larvae::RegisterTest("Math", "Mat4_Inverse", []() {
        Mat4 transform = Translation({5.f, 10.f, 15.f});
        Mat4 inverse = Inverse(transform);
        Mat4 identity = transform * inverse;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                larvae::AssertTrue(Near(identity.m_m[col][row], col == row ? 1.f : 0.f));
    });

    // =========================================================================
    // Transforms
    // =========================================================================

    auto t_translation = larvae::RegisterTest("Math", "Translation", []() {
        Mat4 transform = Translation({1.f, 2.f, 3.f});
        larvae::AssertTrue(Near(transform.m_m[3][0], 1.f));
        larvae::AssertTrue(Near(transform.m_m[3][1], 2.f));
        larvae::AssertTrue(Near(transform.m_m[3][2], 3.f));
        larvae::AssertTrue(Near(transform.m_m[0][0], 1.f));
        larvae::AssertTrue(Near(transform.m_m[1][1], 1.f));
        larvae::AssertTrue(Near(transform.m_m[2][2], 1.f));
        larvae::AssertTrue(Near(transform.m_m[3][3], 1.f));
    });

    auto t_scale = larvae::RegisterTest("Math", "Scale", []() {
        Mat4 scale = Scale({2.f, 3.f, 4.f});
        Float4 point{1.f, 1.f, 1.f, 1.f};
        Float4 result = scale * point;
        larvae::AssertTrue(Near(result.m_x, 2.f));
        larvae::AssertTrue(Near(result.m_y, 3.f));
        larvae::AssertTrue(Near(result.m_z, 4.f));
    });

    auto t_perspective = larvae::RegisterTest("Math", "Perspective", []() {
        Mat4 projection = Perspective(Radians(90.f), 1.f, 0.1f, 100.f);
        larvae::AssertTrue(Near(projection.m_m[0][0], 1.f));
        larvae::AssertTrue(Near(projection.m_m[1][1], -1.f));
        larvae::AssertTrue(Near(projection.m_m[2][3], -1.f));
    });

    auto t_lookat = larvae::RegisterTest("Math", "LookAt", []() {
        Mat4 view = LookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
        Float4 origin{0.f, 0.f, 0.f, 1.f};
        Float4 result = view * origin;
        larvae::AssertTrue(Near(result.m_x, 0.f));
        larvae::AssertTrue(Near(result.m_y, 0.f));
        larvae::AssertTrue(Near(result.m_z, -5.f));
    });

    auto t_trs = larvae::RegisterTest("Math", "TRS", []() {
        Float3 pos{1.f, 2.f, 3.f};
        Quat rot{0.f, 0.f, 0.f, 1.f};
        Float3 scl{2.f, 2.f, 2.f};

        Mat4 transform = TRS(pos, rot, scl);
        Float4 point{1.f, 0.f, 0.f, 1.f};
        Float4 result = transform * point;
        larvae::AssertTrue(Near(result.m_x, 3.f));
        larvae::AssertTrue(Near(result.m_y, 2.f));
        larvae::AssertTrue(Near(result.m_z, 3.f));
    });

    // =========================================================================
    // Quaternion
    // =========================================================================

    auto t_quat_identity = larvae::RegisterTest("Math", "Quat_Identity", []() {
        Quat quat{0.f, 0.f, 0.f, 1.f};
        Mat4 matrix = QuatToMat4(quat);
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                larvae::AssertTrue(Near(matrix.m_m[col][row], col == row ? 1.f : 0.f));
    });

    auto t_quat_rotation_90 = larvae::RegisterTest("Math", "Quat_Rotation90Y", []() {
        Quat quat = QuatFromAxisAngle({0.f, 1.f, 0.f}, Radians(90.f));
        Float3 vector{1.f, 0.f, 0.f};
        Float3 result = RotateByQuat(vector, quat);
        larvae::AssertTrue(Near(result.m_x, 0.f));
        larvae::AssertTrue(Near(result.m_y, 0.f));
        larvae::AssertTrue(Near(result.m_z, -1.f));
    });

    auto t_quat_normalize = larvae::RegisterTest("Math", "Quat_Normalize", []() {
        Quat quat{1.f, 2.f, 3.f, 4.f};
        Quat normalized = Normalize(quat);
        float length = Sqrt(Dot(normalized, normalized));
        larvae::AssertTrue(Near(length, 1.f));
    });

    auto t_quat_slerp = larvae::RegisterTest("Math", "Quat_Slerp", []() {
        Quat a{0.f, 0.f, 0.f, 1.f};
        Quat b = QuatFromAxisAngle({0.f, 1.f, 0.f}, Radians(90.f));
        Quat half = SLerp(a, 0.5f, b);
        Float3 vector{1.f, 0.f, 0.f};
        Float3 result = RotateByQuat(vector, half);
        larvae::AssertTrue(Near(result.m_x, 0.7071f) || Near(result.m_x, std::cos(Radians(45.f))));
    });

    // =========================================================================
    // Float2
    // =========================================================================

    auto t_f2_ops = larvae::RegisterTest("Math", "Float2_Ops", []() {
        Float2 a{1.f, 2.f};
        Float2 b{3.f, 4.f};
        Float2 result = a + b;
        larvae::AssertTrue(Near(result.m_x, 4.f));
        larvae::AssertTrue(Near(result.m_y, 6.f));

        larvae::AssertTrue(Near(Dot(a, b), 11.f));
        larvae::AssertTrue(Near(Length(Float2{3.f, 4.f}), 5.f));
    });

    // =========================================================================
    // Float4
    // =========================================================================

    auto t_f4_ops = larvae::RegisterTest("Math", "Float4_Ops", []() {
        Float4 a{1.f, 2.f, 3.f, 4.f};
        Float4 b{5.f, 6.f, 7.f, 8.f};
        Float4 result = a + b;
        larvae::AssertTrue(Near(result.m_x, 6.f));
        larvae::AssertTrue(Near(result.m_w, 12.f));

        larvae::AssertTrue(Near(Dot(a, b), 70.f));
    });

    // =========================================================================
    // Determinism: same operations produce identical results
    // =========================================================================

    auto t_determinism = larvae::RegisterTest("Math", "Determinism", []() {
        auto compute = []() {
            Float3 vector{1.23456f, 7.89012f, 3.45678f};
            Mat4 projection = Perspective(Radians(60.f), 16.f / 9.f, 0.1f, 1000.f);
            Mat4 view = LookAt({10.f, 5.f, 10.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
            Mat4 viewProjection = projection * view;
            Float4 result = viewProjection * Float4{vector.m_x, vector.m_y, vector.m_z, 1.f};
            return result;
        };

        Float4 r1 = compute();
        Float4 r2 = compute();

        larvae::AssertEqual(r1.m_x, r2.m_x);
        larvae::AssertEqual(r1.m_y, r2.m_y);
        larvae::AssertEqual(r1.m_z, r2.m_z);
        larvae::AssertEqual(r1.m_w, r2.m_w);
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
