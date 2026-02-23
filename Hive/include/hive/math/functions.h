#pragma once

#include <hive/math/types.h>

// HMM config: radians, SIMD auto-detected
#define HANDMADE_MATH_USE_RADIANS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnested-anon-types"
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <HandmadeMath.h>
#pragma clang diagnostic pop

namespace hive::math
{
    // ── Conversion helpers (zero-cost, optimized away) ──────────────────

    inline HMM_Vec2 ToHMM(Float2 v) noexcept { return HMM_V2(v.x, v.y); }
    inline HMM_Vec3 ToHMM(Float3 v) noexcept { return HMM_V3(v.x, v.y, v.z); }
    inline HMM_Vec4 ToHMM(Float4 v) noexcept { return HMM_V4(v.x, v.y, v.z, v.w); }

    inline HMM_Quat ToHMM(Quat q) noexcept { return HMM_Q(q.x, q.y, q.z, q.w); }

    inline HMM_Mat4 ToHMM(const Mat4& m) noexcept
    {
        HMM_Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row)
                r.Elements[c][row] = m.m[c][row];
        return r;
    }

    inline Float2 FromHMM(HMM_Vec2 v) noexcept { return {v.X, v.Y}; }
    inline Float3 FromHMM(HMM_Vec3 v) noexcept { return {v.X, v.Y, v.Z}; }
    inline Float4 FromHMM(HMM_Vec4 v) noexcept { return {v.X, v.Y, v.Z, v.W}; }
    inline Quat   FromHMM(HMM_Quat q) noexcept { return {q.X, q.Y, q.Z, q.W}; }

    inline Mat4 FromHMM(HMM_Mat4 hm) noexcept
    {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row)
                r.m[c][row] = hm.Elements[c][row];
        return r;
    }

    // ── Float2 ops ──────────────────────────────────────────────────────

    [[nodiscard]] inline Float2 operator+(Float2 a, Float2 b) { return FromHMM(HMM_AddV2(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float2 operator-(Float2 a, Float2 b) { return FromHMM(HMM_SubV2(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float2 operator*(Float2 a, Float2 b) { return FromHMM(HMM_MulV2(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float2 operator*(Float2 a, float s)  { return FromHMM(HMM_MulV2F(ToHMM(a), s)); }
    [[nodiscard]] inline Float2 operator*(float s, Float2 a)  { return FromHMM(HMM_MulV2F(ToHMM(a), s)); }
    [[nodiscard]] inline Float2 operator/(Float2 a, float s)  { return FromHMM(HMM_DivV2F(ToHMM(a), s)); }
    [[nodiscard]] inline Float2 operator-(Float2 a)            { return {-a.x, -a.y}; }

    inline Float2& operator+=(Float2& a, Float2 b) { a = a + b; return a; }
    inline Float2& operator-=(Float2& a, Float2 b) { a = a - b; return a; }
    inline Float2& operator*=(Float2& a, float s)  { a = a * s; return a; }

    [[nodiscard]] inline float Dot(Float2 a, Float2 b)       { return HMM_DotV2(ToHMM(a), ToHMM(b)); }
    [[nodiscard]] inline float Length(Float2 a)               { return HMM_LenV2(ToHMM(a)); }
    [[nodiscard]] inline float LengthSq(Float2 a)            { return HMM_LenSqrV2(ToHMM(a)); }
    [[nodiscard]] inline Float2 Normalize(Float2 a)           { return FromHMM(HMM_NormV2(ToHMM(a))); }
    [[nodiscard]] inline Float2 Lerp(Float2 a, float t, Float2 b) { return FromHMM(HMM_LerpV2(ToHMM(a), t, ToHMM(b))); }

    // ── Float3 ops ──────────────────────────────────────────────────────

    [[nodiscard]] inline Float3 operator+(Float3 a, Float3 b) { return FromHMM(HMM_AddV3(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float3 operator-(Float3 a, Float3 b) { return FromHMM(HMM_SubV3(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float3 operator*(Float3 a, Float3 b) { return FromHMM(HMM_MulV3(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float3 operator*(Float3 a, float s)  { return FromHMM(HMM_MulV3F(ToHMM(a), s)); }
    [[nodiscard]] inline Float3 operator*(float s, Float3 a)  { return FromHMM(HMM_MulV3F(ToHMM(a), s)); }
    [[nodiscard]] inline Float3 operator/(Float3 a, float s)  { return FromHMM(HMM_DivV3F(ToHMM(a), s)); }
    [[nodiscard]] inline Float3 operator-(Float3 a)            { return {-a.x, -a.y, -a.z}; }

    inline Float3& operator+=(Float3& a, Float3 b) { a = a + b; return a; }
    inline Float3& operator-=(Float3& a, Float3 b) { a = a - b; return a; }
    inline Float3& operator*=(Float3& a, float s)  { a = a * s; return a; }

    [[nodiscard]] inline float  Dot(Float3 a, Float3 b)       { return HMM_DotV3(ToHMM(a), ToHMM(b)); }
    [[nodiscard]] inline Float3 Cross(Float3 a, Float3 b)     { return FromHMM(HMM_Cross(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline float  Length(Float3 a)               { return HMM_LenV3(ToHMM(a)); }
    [[nodiscard]] inline float  LengthSq(Float3 a)            { return HMM_LenSqrV3(ToHMM(a)); }
    [[nodiscard]] inline Float3 Normalize(Float3 a)            { return FromHMM(HMM_NormV3(ToHMM(a))); }
    [[nodiscard]] inline Float3 Lerp(Float3 a, float t, Float3 b) { return FromHMM(HMM_LerpV3(ToHMM(a), t, ToHMM(b))); }

    // ── Float4 ops ──────────────────────────────────────────────────────

    [[nodiscard]] inline Float4 operator+(Float4 a, Float4 b) { return FromHMM(HMM_AddV4(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float4 operator-(Float4 a, Float4 b) { return FromHMM(HMM_SubV4(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float4 operator*(Float4 a, Float4 b) { return FromHMM(HMM_MulV4(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Float4 operator*(Float4 a, float s)  { return FromHMM(HMM_MulV4F(ToHMM(a), s)); }
    [[nodiscard]] inline Float4 operator*(float s, Float4 a)  { return FromHMM(HMM_MulV4F(ToHMM(a), s)); }
    [[nodiscard]] inline Float4 operator/(Float4 a, float s)  { return FromHMM(HMM_DivV4F(ToHMM(a), s)); }
    [[nodiscard]] inline Float4 operator-(Float4 a)            { return {-a.x, -a.y, -a.z, -a.w}; }

    inline Float4& operator+=(Float4& a, Float4 b) { a = a + b; return a; }
    inline Float4& operator-=(Float4& a, Float4 b) { a = a - b; return a; }
    inline Float4& operator*=(Float4& a, float s)  { a = a * s; return a; }

    [[nodiscard]] inline float  Dot(Float4 a, Float4 b)       { return HMM_DotV4(ToHMM(a), ToHMM(b)); }
    [[nodiscard]] inline float  Length(Float4 a)               { return HMM_LenV4(ToHMM(a)); }
    [[nodiscard]] inline float  LengthSq(Float4 a)            { return HMM_LenSqrV4(ToHMM(a)); }
    [[nodiscard]] inline Float4 Normalize(Float4 a)            { return FromHMM(HMM_NormV4(ToHMM(a))); }
    [[nodiscard]] inline Float4 Lerp(Float4 a, float t, Float4 b) { return FromHMM(HMM_LerpV4(ToHMM(a), t, ToHMM(b))); }

    // ── Mat4 ops ────────────────────────────────────────────────────────

    [[nodiscard]] inline Mat4 operator*(const Mat4& a, const Mat4& b)
    {
        return FromHMM(HMM_MulM4(ToHMM(a), ToHMM(b)));
    }

    [[nodiscard]] inline Float4 operator*(const Mat4& m, Float4 v)
    {
        return FromHMM(HMM_MulM4V4(ToHMM(m), ToHMM(v)));
    }

    [[nodiscard]] inline Mat4 Transpose(const Mat4& m)
    {
        return FromHMM(HMM_TransposeM4(ToHMM(m)));
    }

    [[nodiscard]] inline Mat4 Inverse(const Mat4& m)
    {
        return FromHMM(HMM_InvGeneralM4(ToHMM(m)));
    }

    [[nodiscard]] inline float Determinant(const Mat4& m)
    {
        return HMM_DeterminantM4(ToHMM(m));
    }

    // ── Quaternion ops ──────────────────────────────────────────────────

    [[nodiscard]] inline Quat operator*(Quat a, Quat b) { return FromHMM(HMM_MulQ(ToHMM(a), ToHMM(b))); }
    [[nodiscard]] inline Quat operator*(Quat q, float s) { return FromHMM(HMM_MulQF(ToHMM(q), s)); }

    [[nodiscard]] inline float Dot(Quat a, Quat b)  { return HMM_DotQ(ToHMM(a), ToHMM(b)); }
    [[nodiscard]] inline Quat  Normalize(Quat q)    { return FromHMM(HMM_NormQ(ToHMM(q))); }
    [[nodiscard]] inline Quat  Inverse(Quat q)      { return FromHMM(HMM_InvQ(ToHMM(q))); }
    [[nodiscard]] inline Quat  NLerp(Quat a, float t, Quat b) { return FromHMM(HMM_NLerp(ToHMM(a), t, ToHMM(b))); }
    [[nodiscard]] inline Quat  SLerp(Quat a, float t, Quat b) { return FromHMM(HMM_SLerp(ToHMM(a), t, ToHMM(b))); }

    [[nodiscard]] inline Mat4 QuatToMat4(Quat q)
    {
        return FromHMM(HMM_QToM4(ToHMM(q)));
    }

    // Right-handed (Vulkan)
    [[nodiscard]] inline Quat QuatFromAxisAngle(Float3 axis, float angle_rad)
    {
        return FromHMM(HMM_QFromAxisAngle_RH(ToHMM(axis), angle_rad));
    }

    [[nodiscard]] inline Float3 RotateByQuat(Float3 v, Quat q)
    {
        return FromHMM(HMM_RotateV3Q(ToHMM(v), ToHMM(q)));
    }

    // ── Utility ─────────────────────────────────────────────────────────

    [[nodiscard]] inline float Sqrt(float v) { return HMM_SqrtF(v); }
    [[nodiscard]] inline float InvSqrt(float v) { return HMM_InvSqrtF(v); }
    [[nodiscard]] inline float Sin(float rad) { return HMM_SinF(rad); }
    [[nodiscard]] inline float Cos(float rad) { return HMM_CosF(rad); }
    [[nodiscard]] inline float Tan(float rad) { return HMM_TanF(rad); }
    [[nodiscard]] inline float ACos(float v) { return HMM_ACosF(v); }
    [[nodiscard]] inline float Clamp(float min, float v, float max) { return HMM_Clamp(min, v, max); }

    [[nodiscard]] inline float Min(float a, float b) { return a < b ? a : b; }
    [[nodiscard]] inline float Max(float a, float b) { return a > b ? a : b; }
    [[nodiscard]] inline float Abs(float a) { return a >= 0.f ? a : -a; }
}
