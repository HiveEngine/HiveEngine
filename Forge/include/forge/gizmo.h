#pragma once

#include <hive/math/types.h>

#include <cstdint>

namespace forge
{
    class UndoStack;

    enum class GizmoMode : uint8_t
    {
        TRANSLATE,
        ROTATE,
        SCALE,
    };

    enum class GizmoSpace : uint8_t
    {
        WORLD,
        LOCAL,
    };

    struct GizmoState
    {
        GizmoMode m_mode{GizmoMode::TRANSLATE};
        GizmoSpace m_space{GizmoSpace::WORLD};
        bool m_isUsing{false}; // true while user is dragging a gizmo
    };

    // Draw the gizmo for the selected entity.
    // Call after ImGui::Image in the viewport panel, while the viewport window is current.
    //
    // world_matrix: 4x4 column-major world transform (read/write)
    // out_position/out_rotation_quat/out_scale: decomposed transform written on manipulation
    //
    // Returns true if the transform was modified this frame.
    bool DrawGizmo(GizmoState& state, const hive::math::Mat4& view, const hive::math::Mat4& projection,
                   float* worldMatrix,
                   float* outPosition,     // float[3]
                   float* outRotationQuat, // float[4] (x,y,z,w)
                   float* outScale,        // float[3]
                   float viewportX, float viewportY, float viewportW, float viewportH);
} // namespace forge
