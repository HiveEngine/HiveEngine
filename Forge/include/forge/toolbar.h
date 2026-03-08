#pragma once

#include <forge/gizmo.h>

namespace forge
{
    enum class PlayState : uint8_t
    {
        EDITING,
        PLAYING,
        PAUSED
    };

    struct ToolbarAction
    {
        bool m_playPressed = false;
        bool m_pausePressed = false;
        bool m_stopPressed = false;
    };

    // Draw toolbar buttons inline (no Begin/End — caller provides the context).
    // Call this between BeginMenuBar/EndMenuBar or in any horizontal layout.
    ToolbarAction DrawToolbarButtons(PlayState playState, GizmoState& gizmo);
} // namespace forge
