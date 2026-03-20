#include <forge/progress_overlay.h>

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace forge
{
    ProgressOverlay::ProgressOverlay(QWidget* parent)
        : QDialog{parent, Qt::FramelessWindowHint | Qt::Dialog}
    {
        setModal(true);
        setFixedSize(380, 220);
        setStyleSheet("QDialog { background: #1a1a1a; border: 1px solid #333; border-radius: 12px; }");

        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(36, 28, 36, 28);
        layout->setSpacing(10);

        // Spacer for spinner area (painted in paintEvent)
        layout->addSpacing(56);

        m_titleLabel = new QLabel{this};
        m_titleLabel->setAlignment(Qt::AlignCenter);
        m_titleLabel->setStyleSheet("color: #e8e8e8; font-size: 13pt; font-weight: bold; background: transparent;");
        layout->addWidget(m_titleLabel);

        layout->addSpacing(6);

        m_stepLabel = new QLabel{this};
        m_stepLabel->setAlignment(Qt::AlignLeft);
        m_stepLabel->setStyleSheet("color: #888; font-size: 9pt; background: transparent;");
        layout->addWidget(m_stepLabel);

        m_bar = new QProgressBar{this};
        m_bar->setFixedHeight(6);
        m_bar->setTextVisible(false);
        m_bar->setRange(0, 100);
        m_bar->setValue(0);
        m_bar->setStyleSheet(
            "QProgressBar { background: #333; border: none; border-radius: 3px; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #f0a500, stop:1 #d48f00); border-radius: 3px; }");
        layout->addWidget(m_bar);

        m_counterLabel = new QLabel{this};
        m_counterLabel->setAlignment(Qt::AlignRight);
        m_counterLabel->setStyleSheet("color: #f0a500; font-size: 9pt; background: transparent;");
        layout->addWidget(m_counterLabel);

        layout->addStretch();

        m_spinTimer = new QTimer{this};
        m_spinTimer->setInterval(16);
        connect(m_spinTimer, &QTimer::timeout, this, [this] {
            m_spinAngle += 3.0;
            update();
        });
    }

    void ProgressOverlay::Show(const QString& title)
    {
        m_titleLabel->setText(title);
        m_stepLabel->setText(" ");
        m_counterLabel->clear();
        m_bar->setValue(0);
        m_spinAngle = 0.0;

        // Center on parent
        if (parentWidget())
        {
            QPoint center = parentWidget()->mapToGlobal(parentWidget()->rect().center());
            move(center.x() - width() / 2, center.y() - height() / 2);
        }

        m_spinTimer->start();
        QDialog::show();
    }

    void ProgressOverlay::SetStep(const QString& stepName)
    {
        m_stepLabel->setText(stepName);
    }

    void ProgressOverlay::SetProgress(int current, int total)
    {
        if (total > 0)
        {
            m_bar->setRange(0, total);
            m_bar->setValue(current);
            m_counterLabel->setText(QString{"%1 / %2"}.arg(current).arg(total));
        }
        else
        {
            m_bar->setRange(0, 0);
            m_counterLabel->setText(QString::number(current));
        }
    }

    void ProgressOverlay::Hide()
    {
        m_spinTimer->stop();
        QDialog::hide();
    }

    void ProgressOverlay::paintEvent(QPaintEvent* event)
    {
        QDialog::paintEvent(event);

        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing);

        // Draw spinning hexagon at top center
        p.save();
        p.translate(width() / 2.0, 40.0);
        p.rotate(m_spinAngle);

        const double r = 18.0;
        QPainterPath hex;
        for (int i = 0; i < 6; ++i)
        {
            double a = (60.0 * i - 30.0) * 3.14159265 / 180.0;
            QPointF pt{r * std::cos(a), r * std::sin(a)};
            if (i == 0)
                hex.moveTo(pt);
            else
                hex.lineTo(pt);
        }
        hex.closeSubpath();

        p.setPen(QPen{QColor{0xf0, 0xa5, 0x00}, 2.5, Qt::SolidLine, Qt::RoundCap});
        p.setBrush(Qt::NoBrush);
        p.drawPath(hex);

        p.setPen(QPen{QColor{0xf0, 0xa5, 0x00, 60}, 2.0});
        p.rotate(30.0);
        p.drawPath(hex);
        p.restore();
    }
} // namespace forge
