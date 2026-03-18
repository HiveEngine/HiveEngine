#pragma once

#include <forge/gizmo.h>

#include <QPushButton>
#include <QToolBar>

#include <cstdint>

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

    private:
        QPushButton* m_playBtn{};
        QPushButton* m_pauseBtn{};
        QPushButton* m_stopBtn{};
        QPushButton* m_moveBtn{};
        QPushButton* m_rotateBtn{};
        QPushButton* m_scaleBtn{};
        QPushButton* m_spaceBtn{};
    };
} // namespace forge
