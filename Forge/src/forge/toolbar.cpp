#include <forge/toolbar.h>

#include <QHBoxLayout>
#include <QKeySequence>
#include <QPainter>
#include <QWidget>

namespace forge
{
    static constexpr int kIconSize = 18;

    static QIcon PaintIcon(const std::function<void(QPainter&, int)>& draw)
    {
        QPixmap pix{kIconSize, kIconSize};
        pix.fill(Qt::transparent);
        QPainter p{&pix};
        p.setRenderHint(QPainter::Antialiasing);
        draw(p, kIconSize);
        p.end();
        return QIcon{pix};
    }

    static QIcon MakeMoveIcon()
    {
        return PaintIcon([](QPainter& p, int s) {
            QColor col{0xbb, 0xbb, 0xbb};
            p.setPen(QPen{col, 1.4});
            p.setBrush(Qt::NoBrush);
            int c = s / 2;
            p.drawLine(c, 3, c, s - 3);
            p.drawLine(3, c, s - 3, c);
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            QPolygonF u;
            u << QPointF(c, 1) << QPointF(c - 3, 5) << QPointF(c + 3, 5);
            p.drawPolygon(u);
            QPolygonF r;
            r << QPointF(s - 1, c) << QPointF(s - 5, c - 3) << QPointF(s - 5, c + 3);
            p.drawPolygon(r);
            QPolygonF d;
            d << QPointF(c, s - 1) << QPointF(c - 3, s - 5) << QPointF(c + 3, s - 5);
            p.drawPolygon(d);
            QPolygonF l;
            l << QPointF(1, c) << QPointF(5, c - 3) << QPointF(5, c + 3);
            p.drawPolygon(l);
        });
    }

    static QIcon MakeRotateIcon()
    {
        return PaintIcon([](QPainter& p, int s) {
            QColor col{0xbb, 0xbb, 0xbb};
            p.setPen(QPen{col, 1.4});
            p.setBrush(Qt::NoBrush);
            p.drawArc(3, 3, s - 6, s - 6, 40 * 16, 280 * 16);
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            int c = s / 2;
            QPolygonF a;
            a << QPointF(c + 5, 2) << QPointF(c + 1, 6) << QPointF(c + 7, 5);
            p.drawPolygon(a);
        });
    }

    static QIcon MakeScaleIcon()
    {
        return PaintIcon([](QPainter& p, int s) {
            QColor col{0xbb, 0xbb, 0xbb};
            p.setPen(QPen{col, 1.4});
            p.setBrush(Qt::NoBrush);
            p.drawLine(4, s - 4, s - 4, 4);
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            p.drawRect(2, s - 6, 5, 5);
            p.drawRect(s - 7, 2, 5, 5);
        });
    }

    static QIcon MakePlayIcon()
    {
        return PaintIcon([](QPainter& p, int s) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor{0x5a, 0xc2, 0x5a});
            QPolygonF t;
            t << QPointF(5, 3) << QPointF(5, s - 3) << QPointF(s - 3, s / 2.0);
            p.drawPolygon(t);
        });
    }

    static QIcon MakePauseIcon()
    {
        return PaintIcon([](QPainter& p, int s) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor{0xd4, 0xaa, 0x40});
            p.drawRect(4, 3, 3, s - 6);
            p.drawRect(s - 7, 3, 3, s - 6);
        });
    }

    static QIcon MakeStopIcon()
    {
        return PaintIcon([](QPainter& p, int s) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor{0xd4, 0x55, 0x55});
            p.drawRoundedRect(4, 4, s - 8, s - 8, 1, 1);
        });
    }

    static QIcon MakeBuildIcon()
    {
        return PaintIcon([](QPainter& p, int s) {
            p.setPen(Qt::NoPen);
            // handle
            p.setBrush(QColor{0x8a, 0x77, 0x55});
            p.save();
            p.translate(s / 2.0, s / 2.0);
            p.rotate(45);
            p.drawRect(-1, -1, 3, 10);
            p.restore();
            // head
            p.setBrush(QColor{0x88, 0xaa, 0xdd});
            p.save();
            p.translate(s / 2.0, s / 2.0);
            p.rotate(45);
            p.drawRoundedRect(-5, -7, 11, 5, 1, 1);
            p.restore();
        });
    }

    static constexpr const char* kBtn =
        "QPushButton { background: transparent; border: 1px solid transparent;"
        "  border-radius: 3px; padding: 4px 5px; }"
        "QPushButton:hover { background: #2a2a2a; border-color: #3a3a3a; }"
        "QPushButton:pressed { background: #222; }";

    static constexpr const char* kBtnActive =
        "QPushButton { background: #2a2a2a; border: 1px solid #444;"
        "  border-radius: 3px; padding: 4px 5px; }"
        "QPushButton:hover { background: #333; }";

    static constexpr const char* kSpaceBtn =
        "QPushButton { background: transparent; color: #777; border: 1px solid transparent;"
        "  border-radius: 3px; padding: 3px 8px; font-size: 10px; }"
        "QPushButton:hover { background: #2a2a2a; color: #bbb; border-color: #3a3a3a; }";

    EditorToolbar::EditorToolbar(QWidget* parent)
        : QToolBar{parent}
    {
        setStyleSheet("QToolBar { background: #141414; border-bottom: 1px solid #2a2a2a;"
                      "  spacing: 0px; padding: 0px; margin: 0px; }");
        setMovable(false);
        setFloatable(false);
        setFixedHeight(32);
        setIconSize(QSize{kIconSize, kIconSize});

        auto* container = new QWidget{this};
        auto* layout = new QHBoxLayout{container};
        layout->setContentsMargins(8, 0, 8, 0);
        layout->setSpacing(1);

        // Left: gizmo tools
        m_moveBtn = new QPushButton{this};
        m_moveBtn->setIcon(MakeMoveIcon());
        m_moveBtn->setToolTip("Translate (W)");
        m_rotateBtn = new QPushButton{this};
        m_rotateBtn->setIcon(MakeRotateIcon());
        m_rotateBtn->setToolTip("Rotate (E)");
        m_scaleBtn = new QPushButton{this};
        m_scaleBtn->setIcon(MakeScaleIcon());
        m_scaleBtn->setToolTip("Scale (R)");
        m_spaceBtn = new QPushButton{"World", this};
        m_spaceBtn->setToolTip("Toggle World / Local space");

        m_moveBtn->setStyleSheet(kBtn);
        m_rotateBtn->setStyleSheet(kBtn);
        m_scaleBtn->setStyleSheet(kBtn);
        m_spaceBtn->setStyleSheet(kSpaceBtn);

        layout->addWidget(m_moveBtn);
        layout->addWidget(m_rotateBtn);
        layout->addWidget(m_scaleBtn);
        layout->addSpacing(4);
        layout->addWidget(m_spaceBtn);

        layout->addSpacing(12);

        m_playBtn = new QPushButton{this};
        m_playBtn->setIcon(MakePlayIcon());
        m_playBtn->setToolTip("Play (Ctrl+P)");
        m_pauseBtn = new QPushButton{this};
        m_pauseBtn->setIcon(MakePauseIcon());
        m_pauseBtn->setToolTip("Pause");
        m_stopBtn = new QPushButton{this};
        m_stopBtn->setIcon(MakeStopIcon());
        m_stopBtn->setToolTip("Stop");

        m_playBtn->setStyleSheet(kBtn);
        m_pauseBtn->setStyleSheet(kBtn);
        m_stopBtn->setStyleSheet(kBtn);

        layout->addWidget(m_playBtn);
        layout->addWidget(m_pauseBtn);
        layout->addWidget(m_stopBtn);

        layout->addSpacing(12);

        m_buildBtn = new QPushButton{this};
        m_buildBtn->setIcon(MakeBuildIcon());
        m_buildBtn->setToolTip("Build Gameplay Module (Ctrl+B)");
        m_buildBtn->setStyleSheet(kBtn);
        m_buildBtn->setShortcut(QKeySequence{"Ctrl+B"});

        layout->addWidget(m_buildBtn);

        layout->addStretch();

        addWidget(container);

        connect(m_buildBtn, &QPushButton::clicked, this, &EditorToolbar::buildPressed);
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
                m_playBtn->setIcon(MakePlayIcon());
                m_playBtn->setToolTip("Play (Ctrl+P)");
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
                m_playBtn->setIcon(MakePlayIcon());
                m_playBtn->setToolTip("Resume");
                m_playBtn->setVisible(true);
                m_pauseBtn->setVisible(false);
                m_stopBtn->setVisible(true);
                break;
        }
    }

    void EditorToolbar::SetGizmoState(GizmoState& state)
    {
        m_moveBtn->setStyleSheet(state.m_mode == GizmoMode::TRANSLATE ? kBtnActive : kBtn);
        m_rotateBtn->setStyleSheet(state.m_mode == GizmoMode::ROTATE ? kBtnActive : kBtn);
        m_scaleBtn->setStyleSheet(state.m_mode == GizmoMode::SCALE ? kBtnActive : kBtn);

        m_spaceBtn->setText(state.m_space == GizmoSpace::WORLD ? "World" : "Local");
    }
} // namespace forge
