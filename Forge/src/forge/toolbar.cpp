#include <forge/toolbar.h>

namespace forge
{
    static constexpr const char* kGreenStyle =
        "QPushButton { background-color: #4CAF50; color: white; border: none; padding: 4px 12px; }"
        "QPushButton:hover { background-color: #5CBF60; }"
        "QPushButton:pressed { background-color: #3C9F40; }";

    static constexpr const char* kRedStyle =
        "QPushButton { background-color: #D32F2F; color: white; border: none; padding: 4px 12px; }"
        "QPushButton:hover { background-color: #E34F4F; }"
        "QPushButton:pressed { background-color: #B31F1F; }";

    static constexpr const char* kAmberStyle =
        "QPushButton { background-color: #E6AD0D; color: white; border: none; padding: 4px 12px; }"
        "QPushButton:hover { background-color: #F6BD1D; }"
        "QPushButton:pressed { background-color: #C69D00; }";

    static constexpr const char* kAccentStyle =
        "QPushButton { background-color: #0078D4; color: white; border: none; padding: 4px 12px; }"
        "QPushButton:hover { background-color: #1088E4; }"
        "QPushButton:pressed { background-color: #0068C4; }";

    static constexpr const char* kInactiveStyle =
        "QPushButton { background-color: #262626; color: #B3B3B3; border: none; padding: 4px 12px; }"
        "QPushButton:hover { background-color: #404040; }"
        "QPushButton:pressed { background-color: #0078D4; }";

    EditorToolbar::EditorToolbar(QWidget* parent)
        : QToolBar{parent}
    {
        m_playBtn = new QPushButton{"Play", this};
        m_pauseBtn = new QPushButton{"Pause", this};
        m_stopBtn = new QPushButton{"Stop", this};

        m_playBtn->setStyleSheet(kGreenStyle);
        m_pauseBtn->setStyleSheet(kAmberStyle);
        m_stopBtn->setStyleSheet(kRedStyle);

        addWidget(m_playBtn);
        addWidget(m_pauseBtn);
        addWidget(m_stopBtn);
        addSeparator();

        m_moveBtn = new QPushButton{"Move", this};
        m_rotateBtn = new QPushButton{"Rotate", this};
        m_scaleBtn = new QPushButton{"Scale", this};

        addWidget(m_moveBtn);
        addWidget(m_rotateBtn);
        addWidget(m_scaleBtn);
        addSeparator();

        m_spaceBtn = new QPushButton{"World", this};
        m_spaceBtn->setStyleSheet(kInactiveStyle);
        addWidget(m_spaceBtn);

        connect(m_playBtn, &QPushButton::clicked, this, &EditorToolbar::playPressed);
        connect(m_pauseBtn, &QPushButton::clicked, this, &EditorToolbar::pausePressed);
        connect(m_stopBtn, &QPushButton::clicked, this, &EditorToolbar::stopPressed);

        connect(m_moveBtn, &QPushButton::clicked, this,
                [this]() { emit gizmoModeChanged(static_cast<int>(GizmoMode::TRANSLATE)); });
        connect(m_rotateBtn, &QPushButton::clicked, this,
                [this]() { emit gizmoModeChanged(static_cast<int>(GizmoMode::ROTATE)); });
        connect(m_scaleBtn, &QPushButton::clicked, this,
                [this]() { emit gizmoModeChanged(static_cast<int>(GizmoMode::SCALE)); });
        connect(m_spaceBtn, &QPushButton::clicked, this, &EditorToolbar::gizmoSpaceToggled);

        SetPlayState(PlayState::EDITING);
    }

    void EditorToolbar::SetPlayState(PlayState state)
    {
        switch (state)
        {
            case PlayState::EDITING:
                m_playBtn->setText("Play");
                m_playBtn->setStyleSheet(kGreenStyle);
                m_playBtn->setVisible(true);
                m_pauseBtn->setVisible(false);
                m_stopBtn->setVisible(false);
                break;
            case PlayState::PLAYING:
                m_playBtn->setVisible(false);
                m_pauseBtn->setVisible(true);
                m_stopBtn->setVisible(true);
                break;
            case PlayState::PAUSED:
                m_playBtn->setText("Resume");
                m_playBtn->setStyleSheet(kGreenStyle);
                m_playBtn->setVisible(true);
                m_pauseBtn->setVisible(false);
                m_stopBtn->setVisible(true);
                break;
        }
    }

    void EditorToolbar::SetGizmoState(GizmoState& state)
    {
        m_moveBtn->setStyleSheet(state.m_mode == GizmoMode::TRANSLATE ? kAccentStyle : kInactiveStyle);
        m_rotateBtn->setStyleSheet(state.m_mode == GizmoMode::ROTATE ? kAccentStyle : kInactiveStyle);
        m_scaleBtn->setStyleSheet(state.m_mode == GizmoMode::SCALE ? kAccentStyle : kInactiveStyle);

        m_spaceBtn->setText(state.m_space == GizmoSpace::WORLD ? "World" : "Local");
    }
} // namespace forge
