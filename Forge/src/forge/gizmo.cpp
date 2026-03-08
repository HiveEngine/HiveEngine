#include <hive/math/math.h>

#include <forge/gizmo.h>

#include <imgui.h>

#include <ImGuizmo.h>

#include <cmath>
#include <cstring>

namespace forge
{
    using namespace hive::math;

    static void EulerToQuat(const float* eulerDeg, float* quat) {
        float px = eulerDeg[0] * (kPi / 180.f) * 0.5f;
        float py = eulerDeg[1] * (kPi / 180.f) * 0.5f;
        float pz = eulerDeg[2] * (kPi / 180.f) * 0.5f;

        float cx = std::cos(px), sx = std::sin(px);
        float cy = std::cos(py), sy = std::sin(py);
        float cz = std::cos(pz), sz = std::sin(pz);

        quat[0] = sx * cy * cz - cx * sy * sz; // x
        quat[1] = cx * sy * cz + sx * cy * sz; // y
        quat[2] = cx * cy * sz - sx * sy * cz; // z
        quat[3] = cx * cy * cz + sx * sy * sz; // w
    }

    bool DrawGizmo(GizmoState& state, const Mat4& view, const Mat4& projection, float* worldMatrix, float* outPosition,
                   float* outRotationQuat, float* outScale, float viewportX, float viewportY, float viewportW,
                   float viewportH) {
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(viewportX, viewportY, viewportW, viewportH);

        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
        switch (state.m_mode)
        {
            case GizmoMode::TRANSLATE:
                op = ImGuizmo::TRANSLATE;
                break;
            case GizmoMode::ROTATE:
                op = ImGuizmo::ROTATE;
                break;
            case GizmoMode::SCALE:
                op = ImGuizmo::SCALE;
                break;
        }

        ImGuizmo::MODE mode = (state.m_space == GizmoSpace::LOCAL) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        bool manipulated = ImGuizmo::Manipulate(&view.m_m[0][0], &projection.m_m[0][0], op, mode, worldMatrix);

        state.m_isUsing = ImGuizmo::IsUsing();

        if (manipulated)
        {
            float t[3], r[3], s[3];
            ImGuizmo::DecomposeMatrixToComponents(worldMatrix, t, r, s);

            std::memcpy(outPosition, t, sizeof(float) * 3);
            std::memcpy(outScale, s, sizeof(float) * 3);
            EulerToQuat(r, outRotationQuat);
        }

        return manipulated;
    }
} // namespace forge
