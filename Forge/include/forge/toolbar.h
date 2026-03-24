#pragma once

#include <hive/math/types.h>

#include <QPushButton>
#include <QToolBar>

#include <cstdint>

namespace forge
{
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
        bool m_isUsing{false};
    };

    bool DrawGizmo(GizmoState& state, const hive::math::Mat4& view, const hive::math::Mat4& projection,
                   float* worldMatrix,
                   float* outPosition,     // float[3]
                   float* outRotationQuat, // float[4] (x,y,z,w)
                   float* outScale,        // float[3]
                   float viewportX, float viewportY, float viewportW, float viewportH);

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

    class EditorToolbar : public QToolBar
    {
        Q_OBJECT

    public:
        explicit EditorToolbar(QWidget* parent = nullptr);

        void SetPlayState(PlayState state);
        void SetGizmoState(GizmoState& state);

    signals:
        void playPressed();
        void pausePressed();
        void stopPressed();
        void gizmoModeChanged(int mode);
        void gizmoSpaceToggled();
        void buildPressed();

    private:
        QPushButton* m_playBtn{};
        QPushButton* m_pauseBtn{};
        QPushButton* m_stopBtn{};
        QPushButton* m_moveBtn{};
        QPushButton* m_rotateBtn{};
        QPushButton* m_scaleBtn{};
        QPushButton* m_spaceBtn{};
        QPushButton* m_buildBtn{};
    };
} // namespace forge
