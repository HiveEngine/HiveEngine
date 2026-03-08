#pragma once

#include <hive/math/functions.h>

namespace hive::math
{
    [[nodiscard]] inline Mat4 PerspectiveRhZo(float fovRad, float aspect, float zNear, float zFar)
    {
        Mat4 result = FromHMM(HMM_Perspective_RH_ZO(fovRad, aspect, zNear, zFar));
        result.m_m[1][1] *= -1.f;
        return result;
    }

    [[nodiscard]] inline Mat4 PerspectiveRhNo(float fovRad, float aspect, float zNear, float zFar)
    {
        return FromHMM(HMM_Perspective_RH_NO(fovRad, aspect, zNear, zFar));
    }

    [[nodiscard]] inline Mat4 PerspectiveLhZo(float fovRad, float aspect, float zNear, float zFar)
    {
        return FromHMM(HMM_Perspective_LH_ZO(fovRad, aspect, zNear, zFar));
    }

    [[nodiscard]] inline Mat4 PerspectiveLhNo(float fovRad, float aspect, float zNear, float zFar)
    {
        return FromHMM(HMM_Perspective_LH_NO(fovRad, aspect, zNear, zFar));
    }

    [[nodiscard]] inline Mat4 Perspective(float fovRad, float aspect, float zNear, float zFar)
    {
        return PerspectiveRhZo(fovRad, aspect, zNear, zFar);
    }

    [[nodiscard]] inline Mat4 OrthographicRhZo(float left, float right, float bottom, float top, float zNear,
                                               float zFar)
    {
        Mat4 result = FromHMM(HMM_Orthographic_RH_ZO(left, right, bottom, top, zNear, zFar));
        result.m_m[1][1] *= -1.f;
        return result;
    }

    [[nodiscard]] inline Mat4 OrthographicRhNo(float left, float right, float bottom, float top, float zNear,
                                               float zFar)
    {
        return FromHMM(HMM_Orthographic_RH_NO(left, right, bottom, top, zNear, zFar));
    }

    [[nodiscard]] inline Mat4 OrthographicLhZo(float left, float right, float bottom, float top, float zNear,
                                               float zFar)
    {
        return FromHMM(HMM_Orthographic_LH_ZO(left, right, bottom, top, zNear, zFar));
    }

    [[nodiscard]] inline Mat4 OrthographicLhNo(float left, float right, float bottom, float top, float zNear,
                                               float zFar)
    {
        return FromHMM(HMM_Orthographic_LH_NO(left, right, bottom, top, zNear, zFar));
    }

    [[nodiscard]] inline Mat4 Orthographic(float left, float right, float bottom, float top, float zNear, float zFar)
    {
        return OrthographicRhZo(left, right, bottom, top, zNear, zFar);
    }

    [[nodiscard]] inline Mat4 LookAtRh(Float3 eye, Float3 target, Float3 up)
    {
        return FromHMM(HMM_LookAt_RH(ToHMM(eye), ToHMM(target), ToHMM(up)));
    }

    [[nodiscard]] inline Mat4 LookAtLh(Float3 eye, Float3 target, Float3 up)
    {
        return FromHMM(HMM_LookAt_LH(ToHMM(eye), ToHMM(target), ToHMM(up)));
    }

    [[nodiscard]] inline Mat4 LookAt(Float3 eye, Float3 target, Float3 up)
    {
        return LookAtRh(eye, target, up);
    }

    [[nodiscard]] inline Mat4 Translation(Float3 t)
    {
        return FromHMM(HMM_Translate(ToHMM(t)));
    }

    [[nodiscard]] inline Mat4 Rotation(Quat q)
    {
        return QuatToMat4(q);
    }

    [[nodiscard]] inline Mat4 RotationRh(Float3 axis, float angleRad)
    {
        return FromHMM(HMM_Rotate_RH(angleRad, ToHMM(axis)));
    }

    [[nodiscard]] inline Mat4 RotationLh(Float3 axis, float angleRad)
    {
        return FromHMM(HMM_Rotate_LH(angleRad, ToHMM(axis)));
    }

    [[nodiscard]] inline Mat4 Rotation(Float3 axis, float angleRad)
    {
        return RotationRh(axis, angleRad);
    }

    [[nodiscard]] inline Mat4 Scale(Float3 s)
    {
        return FromHMM(HMM_Scale(ToHMM(s)));
    }

    [[nodiscard]] inline Mat4 TRS(Float3 pos, Quat rot, Float3 scl)
    {
        return Translation(pos) * Rotation(rot) * Scale(scl);
    }
} // namespace hive::math
