#pragma once

#include <forge/gizmo.h>

namespace forge
{
    enum class PlayState : uint8_t
    {
        Editing,
        Playing,
        Paused
    };

    struct ToolbarAction
    {
        bool play_pressed  = false;
        bool pause_pressed = false;
        bool stop_pressed  = false;
    };

    // Draw toolbar buttons inline (no Begin/End — caller provides the context).
    // Call this between BeginMenuBar/EndMenuBar or in any horizontal layout.
    ToolbarAction DrawToolbarButtons(PlayState play_state, GizmoState& gizmo);
}
