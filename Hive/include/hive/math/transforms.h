#pragma once

#include <hive/math/functions.h>

namespace hive::math
{
    // Coordinate system conventions:
    //   RH = right-handed (Vulkan, OpenGL)
    //   LH = left-handed  (D3D12, Metal)
    //   ZO = depth [0, 1] (Vulkan, D3D12, Metal)
    //   NO = depth [-1, 1] (OpenGL)
    //
    // Default functions (Perspective, Orthographic, LookAt, Rotation) use RH_ZO (Vulkan).
    // Explicit variants available for other backends.

    // ── Perspective ──────────────────────────────────────────────────────

    [[nodiscard]] inline Mat4 Perspective_RH_ZO(float fov_rad, float aspect, float z_near, float z_far)
    {
        Mat4 r = FromHMM(HMM_Perspective_RH_ZO(fov_rad, aspect, z_near, z_far));
        r.m[1][1] *= -1.f; // Vulkan NDC Y points down
        return r;
    }

    [[nodiscard]] inline Mat4 Perspective_RH_NO(float fov_rad, float aspect, float z_near, float z_far)
    {
        return FromHMM(HMM_Perspective_RH_NO(fov_rad, aspect, z_near, z_far));
    }

    [[nodiscard]] inline Mat4 Perspective_LH_ZO(float fov_rad, float aspect, float z_near, float z_far)
    {
        return FromHMM(HMM_Perspective_LH_ZO(fov_rad, aspect, z_near, z_far));
    }

    [[nodiscard]] inline Mat4 Perspective_LH_NO(float fov_rad, float aspect, float z_near, float z_far)
    {
        return FromHMM(HMM_Perspective_LH_NO(fov_rad, aspect, z_near, z_far));
    }

    // Default: Vulkan (RH_ZO)
    [[nodiscard]] inline Mat4 Perspective(float fov_rad, float aspect, float z_near, float z_far)
    {
        return Perspective_RH_ZO(fov_rad, aspect, z_near, z_far);
    }

    // ── Orthographic ─────────────────────────────────────────────────────

    [[nodiscard]] inline Mat4 Orthographic_RH_ZO(float left, float right, float bottom, float top, float z_near, float z_far)
    {
        Mat4 r = FromHMM(HMM_Orthographic_RH_ZO(left, right, bottom, top, z_near, z_far));
        r.m[1][1] *= -1.f; // Vulkan NDC Y points down
        return r;
    }

    [[nodiscard]] inline Mat4 Orthographic_RH_NO(float left, float right, float bottom, float top, float z_near, float z_far)
    {
        return FromHMM(HMM_Orthographic_RH_NO(left, right, bottom, top, z_near, z_far));
    }

    [[nodiscard]] inline Mat4 Orthographic_LH_ZO(float left, float right, float bottom, float top, float z_near, float z_far)
    {
        return FromHMM(HMM_Orthographic_LH_ZO(left, right, bottom, top, z_near, z_far));
    }

    [[nodiscard]] inline Mat4 Orthographic_LH_NO(float left, float right, float bottom, float top, float z_near, float z_far)
    {
        return FromHMM(HMM_Orthographic_LH_NO(left, right, bottom, top, z_near, z_far));
    }

    // Default: Vulkan (RH_ZO)
    [[nodiscard]] inline Mat4 Orthographic(float left, float right, float bottom, float top, float z_near, float z_far)
    {
        return Orthographic_RH_ZO(left, right, bottom, top, z_near, z_far);
    }

    // ── LookAt ───────────────────────────────────────────────────────────

    [[nodiscard]] inline Mat4 LookAt_RH(Float3 eye, Float3 target, Float3 up)
    {
        return FromHMM(HMM_LookAt_RH(ToHMM(eye), ToHMM(target), ToHMM(up)));
    }

    [[nodiscard]] inline Mat4 LookAt_LH(Float3 eye, Float3 target, Float3 up)
    {
        return FromHMM(HMM_LookAt_LH(ToHMM(eye), ToHMM(target), ToHMM(up)));
    }

    // Default: RH
    [[nodiscard]] inline Mat4 LookAt(Float3 eye, Float3 target, Float3 up)
    {
        return LookAt_RH(eye, target, up);
    }

    // ── Model transforms (handedness-independent) ────────────────────────

    [[nodiscard]] inline Mat4 Translation(Float3 t)
    {
        return FromHMM(HMM_Translate(ToHMM(t)));
    }

    [[nodiscard]] inline Mat4 Rotation(Quat q)
    {
        return QuatToMat4(q);
    }

    [[nodiscard]] inline Mat4 Rotation_RH(Float3 axis, float angle_rad)
    {
        return FromHMM(HMM_Rotate_RH(angle_rad, ToHMM(axis)));
    }

    [[nodiscard]] inline Mat4 Rotation_LH(Float3 axis, float angle_rad)
    {
        return FromHMM(HMM_Rotate_LH(angle_rad, ToHMM(axis)));
    }

    // Default: RH
    [[nodiscard]] inline Mat4 Rotation(Float3 axis, float angle_rad)
    {
        return Rotation_RH(axis, angle_rad);
    }

    [[nodiscard]] inline Mat4 Scale(Float3 s)
    {
        return FromHMM(HMM_Scale(ToHMM(s)));
    }

    [[nodiscard]] inline Mat4 TRS(Float3 pos, Quat rot, Float3 scl)
    {
        return Translation(pos) * Rotation(rot) * Scale(scl);
    }
}
