#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;
class QTimer;

namespace forge
{
    class ProgressOverlay : public QDialog
    {
        Q_OBJECT

    public:
        explicit ProgressOverlay(QWidget* parent);

        void Show(const QString& title);
        void SetStep(const QString& stepName);
        void SetProgress(int current, int total);
        void Hide();

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        QLabel* m_titleLabel{};
        QLabel* m_stepLabel{};
        QLabel* m_counterLabel{};
        QProgressBar* m_bar{};
        QTimer* m_spinTimer{};
        double m_spinAngle{0.0};
    };
} // namespace forge
