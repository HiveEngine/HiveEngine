#pragma once

#include <hive/math/types.h>

#define HANDMADE_MATH_USE_RADIANS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnested-anon-types"
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <HandmadeMath.h>
#pragma clang diagnostic pop

namespace hive::math
{
    [[nodiscard]] inline HMM_Vec2 ToHMM(Float2 v) noexcept
    {
        return HMM_V2(v.m_x, v.m_y);
    }

    [[nodiscard]] inline HMM_Vec3 ToHMM(Float3 v) noexcept
    {
        return HMM_V3(v.m_x, v.m_y, v.m_z);
    }

    [[nodiscard]] inline HMM_Vec4 ToHMM(Float4 v) noexcept
    {
        return HMM_V4(v.m_x, v.m_y, v.m_z, v.m_w);
    }

    [[nodiscard]] inline HMM_Quat ToHMM(Quat q) noexcept
    {
        return HMM_Q(q.m_x, q.m_y, q.m_z, q.m_w);
    }

    [[nodiscard]] inline HMM_Mat4 ToHMM(const Mat4& m) noexcept
    {
        HMM_Mat4 result{};
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                result.Elements[column][row] = m.m_m[column][row];
            }
        }
        return result;
    }

    [[nodiscard]] inline Float2 FromHMM(HMM_Vec2 v) noexcept
    {
        return {v.X, v.Y};
    }

    [[nodiscard]] inline Float3 FromHMM(HMM_Vec3 v) noexcept
    {
        return {v.X, v.Y, v.Z};
    }

    [[nodiscard]] inline Float4 FromHMM(HMM_Vec4 v) noexcept
    {
        return {v.X, v.Y, v.Z, v.W};
    }

    [[nodiscard]] inline Quat FromHMM(HMM_Quat q) noexcept
    {
        return {q.X, q.Y, q.Z, q.W};
    }

    [[nodiscard]] inline Mat4 FromHMM(HMM_Mat4 hm) noexcept
    {
        Mat4 result{};
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                result.m_m[column][row] = hm.Elements[column][row];
            }
        }
        return result;
    }

    [[nodiscard]] inline Float2 operator+(Float2 a, Float2 b)
    {
        return FromHMM(HMM_AddV2(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float2 operator-(Float2 a, Float2 b)
    {
        return FromHMM(HMM_SubV2(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float2 operator*(Float2 a, Float2 b)
    {
        return FromHMM(HMM_MulV2(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float2 operator*(Float2 a, float s)
    {
        return FromHMM(HMM_MulV2F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float2 operator*(float s, Float2 a)
    {
        return FromHMM(HMM_MulV2F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float2 operator/(Float2 a, float s)
    {
        return FromHMM(HMM_DivV2F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float2 operator-(Float2 a)
    {
        return {-a.m_x, -a.m_y};
    }

    inline Float2& operator+=(Float2& a, Float2 b)
    {
        a = a + b;
        return a;
    }

    inline Float2& operator-=(Float2& a, Float2 b)
    {
        a = a - b;
        return a;
    }

    inline Float2& operator*=(Float2& a, float s)
    {
        a = a * s;
        return a;
    }

    [[nodiscard]] inline float Dot(Float2 a, Float2 b)
    {
        return HMM_DotV2(ToHMM(a), ToHMM(b));
    }
    [[nodiscard]] inline float Length(Float2 a)
    {
        return HMM_LenV2(ToHMM(a));
    }
    [[nodiscard]] inline float LengthSq(Float2 a)
    {
        return HMM_LenSqrV2(ToHMM(a));
    }
    [[nodiscard]] inline Float2 Normalize(Float2 a)
    {
        float lenSq = HMM_LenSqrV2(ToHMM(a));
        if (lenSq < 1e-12f)
            return Float2{0.f, 0.f};
        return FromHMM(HMM_MulV2F(ToHMM(a), HMM_InvSqrtF(lenSq)));
    }
    [[nodiscard]] inline Float2 Lerp(Float2 a, float t, Float2 b)
    {
        return FromHMM(HMM_LerpV2(ToHMM(a), t, ToHMM(b)));
    }

    [[nodiscard]] inline Float3 operator+(Float3 a, Float3 b)
    {
        return FromHMM(HMM_AddV3(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float3 operator-(Float3 a, Float3 b)
    {
        return FromHMM(HMM_SubV3(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float3 operator*(Float3 a, Float3 b)
    {
        return FromHMM(HMM_MulV3(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float3 operator*(Float3 a, float s)
    {
        return FromHMM(HMM_MulV3F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float3 operator*(float s, Float3 a)
    {
        return FromHMM(HMM_MulV3F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float3 operator/(Float3 a, float s)
    {
        return FromHMM(HMM_DivV3F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float3 operator-(Float3 a)
    {
        return {-a.m_x, -a.m_y, -a.m_z};
    }

    inline Float3& operator+=(Float3& a, Float3 b)
    {
        a = a + b;
        return a;
    }

    inline Float3& operator-=(Float3& a, Float3 b)
    {
        a = a - b;
        return a;
    }

    inline Float3& operator*=(Float3& a, float s)
    {
        a = a * s;
        return a;
    }

    [[nodiscard]] inline float Dot(Float3 a, Float3 b)
    {
        return HMM_DotV3(ToHMM(a), ToHMM(b));
    }
    [[nodiscard]] inline Float3 Cross(Float3 a, Float3 b)
    {
        return FromHMM(HMM_Cross(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline float Length(Float3 a)
    {
        return HMM_LenV3(ToHMM(a));
    }
    [[nodiscard]] inline float LengthSq(Float3 a)
    {
        return HMM_LenSqrV3(ToHMM(a));
    }
    [[nodiscard]] inline Float3 Normalize(Float3 a)
    {
        float lenSq = HMM_LenSqrV3(ToHMM(a));
        if (lenSq < 1e-12f)
            return Float3{0.f, 0.f, 0.f};
        return FromHMM(HMM_MulV3F(ToHMM(a), HMM_InvSqrtF(lenSq)));
    }
    [[nodiscard]] inline Float3 Lerp(Float3 a, float t, Float3 b)
    {
        return FromHMM(HMM_LerpV3(ToHMM(a), t, ToHMM(b)));
    }

    [[nodiscard]] inline Float4 operator+(Float4 a, Float4 b)
    {
        return FromHMM(HMM_AddV4(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float4 operator-(Float4 a, Float4 b)
    {
        return FromHMM(HMM_SubV4(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float4 operator*(Float4 a, Float4 b)
    {
        return FromHMM(HMM_MulV4(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Float4 operator*(Float4 a, float s)
    {
        return FromHMM(HMM_MulV4F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float4 operator*(float s, Float4 a)
    {
        return FromHMM(HMM_MulV4F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float4 operator/(Float4 a, float s)
    {
        return FromHMM(HMM_DivV4F(ToHMM(a), s));
    }
    [[nodiscard]] inline Float4 operator-(Float4 a)
    {
        return {-a.m_x, -a.m_y, -a.m_z, -a.m_w};
    }

    inline Float4& operator+=(Float4& a, Float4 b)
    {
        a = a + b;
        return a;
    }

    inline Float4& operator-=(Float4& a, Float4 b)
    {
        a = a - b;
        return a;
    }

    inline Float4& operator*=(Float4& a, float s)
    {
        a = a * s;
        return a;
    }

    [[nodiscard]] inline float Dot(Float4 a, Float4 b)
    {
        return HMM_DotV4(ToHMM(a), ToHMM(b));
    }
    [[nodiscard]] inline float Length(Float4 a)
    {
        return HMM_LenV4(ToHMM(a));
    }
    [[nodiscard]] inline float LengthSq(Float4 a)
    {
        return HMM_LenSqrV4(ToHMM(a));
    }
    [[nodiscard]] inline Float4 Normalize(Float4 a)
    {
        float lenSq = HMM_LenSqrV4(ToHMM(a));
        if (lenSq < 1e-12f)
            return Float4{0.f, 0.f, 0.f, 0.f};
        return FromHMM(HMM_MulV4F(ToHMM(a), HMM_InvSqrtF(lenSq)));
    }
    [[nodiscard]] inline Float4 Lerp(Float4 a, float t, Float4 b)
    {
        return FromHMM(HMM_LerpV4(ToHMM(a), t, ToHMM(b)));
    }

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

    [[nodiscard]] inline Quat operator*(Quat a, Quat b)
    {
        return FromHMM(HMM_MulQ(ToHMM(a), ToHMM(b)));
    }
    [[nodiscard]] inline Quat operator*(Quat q, float s)
    {
        return FromHMM(HMM_MulQF(ToHMM(q), s));
    }
    [[nodiscard]] inline float Dot(Quat a, Quat b)
    {
        return HMM_DotQ(ToHMM(a), ToHMM(b));
    }
    [[nodiscard]] inline Quat Normalize(Quat q)
    {
        float lenSq = HMM_DotQ(ToHMM(q), ToHMM(q));
        if (lenSq < 1e-12f)
            return Quat{0.f, 0.f, 0.f, 1.f};
        HMM_Quat h = ToHMM(q);
        float inv = HMM_InvSqrtF(lenSq);
        h.X *= inv;
        h.Y *= inv;
        h.Z *= inv;
        h.W *= inv;
        return FromHMM(h);
    }
    [[nodiscard]] inline Quat Inverse(Quat q)
    {
        return FromHMM(HMM_InvQ(ToHMM(q)));
    }
    [[nodiscard]] inline Quat NLerp(Quat a, float t, Quat b)
    {
        return FromHMM(HMM_NLerp(ToHMM(a), t, ToHMM(b)));
    }
    [[nodiscard]] inline Quat SLerp(Quat a, float t, Quat b)
    {
        return FromHMM(HMM_SLerp(ToHMM(a), t, ToHMM(b)));
    }
    [[nodiscard]] inline Mat4 QuatToMat4(Quat q)
    {
        return FromHMM(HMM_QToM4(ToHMM(q)));
    }
    [[nodiscard]] inline Quat QuatFromAxisAngle(Float3 axis, float angleRad)
    {
        return FromHMM(HMM_QFromAxisAngle_RH(ToHMM(axis), angleRad));
    }
    [[nodiscard]] inline Float3 RotateByQuat(Float3 v, Quat q)
    {
        return FromHMM(HMM_RotateV3Q(ToHMM(v), ToHMM(q)));
    }

    [[nodiscard]] inline float Sqrt(float v)
    {
        return HMM_SqrtF(v);
    }
    [[nodiscard]] inline float InvSqrt(float v)
    {
        return HMM_InvSqrtF(v);
    }
    [[nodiscard]] inline float Sin(float rad)
    {
        return HMM_SinF(rad);
    }
    [[nodiscard]] inline float Cos(float rad)
    {
        return HMM_CosF(rad);
    }
    [[nodiscard]] inline float Tan(float rad)
    {
        return HMM_TanF(rad);
    }
    [[nodiscard]] inline float ACos(float v)
    {
        return HMM_ACosF(v);
    }
    [[nodiscard]] inline float Clamp(float min, float v, float max)
    {
        return HMM_Clamp(min, v, max);
    }

    [[nodiscard]] inline float Min(float a, float b)
    {
        return a < b ? a : b;
    }
    [[nodiscard]] inline float Max(float a, float b)
    {
        return a > b ? a : b;
    }
    [[nodiscard]] inline float Abs(float a)
    {
        return a >= 0.f ? a : -a;
    }
} // namespace hive::math
