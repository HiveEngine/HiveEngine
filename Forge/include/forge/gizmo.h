#pragma once

#include <hive/math/types.h>

#include <cstdint>

namespace forge
{
    class UndoStack;

    enum class GizmoMode : uint8_t
    {
        Translate,
        Rotate,
        Scale,
    };

    enum class GizmoSpace : uint8_t
    {
        World,
        Local,
    };

    struct GizmoState
    {
        GizmoMode mode{GizmoMode::Translate};
        GizmoSpace space{GizmoSpace::World};
        bool is_using{false};       // true while user is dragging a gizmo
    };

    // Draw the gizmo for the selected entity.
    // Call after ImGui::Image in the viewport panel, while the viewport window is current.
    //
    // world_matrix: 4x4 column-major world transform (read/write)
    // out_position/out_rotation_quat/out_scale: decomposed transform written on manipulation
    //
    // Returns true if the transform was modified this frame.
    bool DrawGizmo(GizmoState& state,
                   const hive::math::Mat4& view,
                   const hive::math::Mat4& projection,
                   float* world_matrix,
                   float* out_position,       // float[3]
                   float* out_rotation_quat,  // float[4] (x,y,z,w)
                   float* out_scale,           // float[3]
                   float viewport_x, float viewport_y,
                   float viewport_w, float viewport_h);
}
