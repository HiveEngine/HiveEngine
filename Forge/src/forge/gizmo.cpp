#include <forge/gizmo.h>

#include <hive/math/math.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <cstring>
#include <cmath>

namespace forge
{
    using namespace hive::math;

    static void EulerToQuat(const float* euler_deg, float* quat)
    {
        float px = euler_deg[0] * (kPi / 180.f) * 0.5f;
        float py = euler_deg[1] * (kPi / 180.f) * 0.5f;
        float pz = euler_deg[2] * (kPi / 180.f) * 0.5f;

        float cx = std::cos(px), sx = std::sin(px);
        float cy = std::cos(py), sy = std::sin(py);
        float cz = std::cos(pz), sz = std::sin(pz);

        quat[0] = sx * cy * cz - cx * sy * sz; // x
        quat[1] = cx * sy * cz + sx * cy * sz; // y
        quat[2] = cx * cy * sz - sx * sy * cz; // z
        quat[3] = cx * cy * cz + sx * sy * sz; // w
    }

    bool DrawGizmo(GizmoState& state,
                   const Mat4& view,
                   const Mat4& projection,
                   float* world_matrix,
                   float* out_position,
                   float* out_rotation_quat,
                   float* out_scale,
                   float viewport_x, float viewport_y,
                   float viewport_w, float viewport_h)
    {
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(viewport_x, viewport_y, viewport_w, viewport_h);

        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
        switch (state.mode)
        {
        case GizmoMode::Translate: op = ImGuizmo::TRANSLATE; break;
        case GizmoMode::Rotate:    op = ImGuizmo::ROTATE;    break;
        case GizmoMode::Scale:     op = ImGuizmo::SCALE;     break;
        }

        ImGuizmo::MODE mode = (state.space == GizmoSpace::Local)
            ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        bool manipulated = ImGuizmo::Manipulate(
            &view.m[0][0], &projection.m[0][0],
            op, mode, world_matrix);

        state.is_using = ImGuizmo::IsUsing();

        if (manipulated)
        {
            float t[3], r[3], s[3];
            ImGuizmo::DecomposeMatrixToComponents(world_matrix, t, r, s);

            std::memcpy(out_position, t, sizeof(float) * 3);
            std::memcpy(out_scale, s, sizeof(float) * 3);
            EulerToQuat(r, out_rotation_quat);
        }

        return manipulated;
    }
}
