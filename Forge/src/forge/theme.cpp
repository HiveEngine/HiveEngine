#include <forge/theme.h>

#include <QApplication>
#include <QFont>
#include <QPalette>

namespace forge
{
    void ApplyForgeTheme()
    {
        QPalette palette{};

        const QColor background{0x0d, 0x0d, 0x0d};
        const QColor surface{0x1a, 0x1a, 0x1a};
        const QColor widget{0x22, 0x22, 0x22};
        const QColor accent{0xf0, 0xa5, 0x00};
        const QColor text{0xe8, 0xe8, 0xe8};
        const QColor dimText{0x88, 0x88, 0x88};
        const QColor border{0x2a, 0x2a, 0x2a};

        palette.setColor(QPalette::Window, background);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, surface);
        palette.setColor(QPalette::AlternateBase, widget);
        palette.setColor(QPalette::ToolTipBase, widget);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Button, widget);
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, accent);
        palette.setColor(QPalette::Link, accent);
        palette.setColor(QPalette::Highlight, accent);
        palette.setColor(QPalette::HighlightedText, QColor{0x0d, 0x0d, 0x0d});
        palette.setColor(QPalette::Disabled, QPalette::Text, dimText);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, dimText);
        palette.setColor(QPalette::Disabled, QPalette::WindowText, dimText);
        palette.setColor(QPalette::Mid, border);
        palette.setColor(QPalette::Dark, QColor{0x14, 0x14, 0x14});
        palette.setColor(QPalette::Shadow, QColor{0x00, 0x00, 0x00});

        QApplication::setPalette(palette);

        qApp->setStyleSheet(R"(
            QMainWindow, QDockWidget {
                background-color: #0d0d0d;
            }
            QDockWidget {
                color: #e8e8e8;
                titlebar-close-icon: none;
                titlebar-normal-icon: none;
            }
            QDockWidget::title {
                background-color: #141414;
                border: 1px solid #2a2a2a;
                padding: 6px;
                color: #e8e8e8;
            }
            QMenuBar {
                background-color: #141414;
                color: #ccc;
                border-bottom: 1px solid #2a2a2a;
            }
            QMenuBar::item:selected {
                background-color: #f0a500;
                color: #0d0d0d;
            }
            QMenu {
                background-color: #1a1a1a;
                color: #e8e8e8;
                border: 1px solid #2a2a2a;
            }
            QMenu::item:selected {
                background-color: #f0a500;
                color: #0d0d0d;
            }
            QTreeWidget, QTreeView, QListView {
                background-color: #111;
                color: #e8e8e8;
                border: 1px solid #2a2a2a;
                outline: none;
            }
            QTreeWidget::item:hover, QTreeView::item:hover {
                background-color: #1e1e1e;
            }
            QTreeWidget::item:selected, QTreeView::item:selected {
                background-color: #f0a500;
                color: #0d0d0d;
            }
            QScrollBar:vertical {
                background: #111;
                width: 8px;
                border-radius: 4px;
            }
            QScrollBar::handle:vertical {
                background: #333;
                border-radius: 4px;
                min-height: 30px;
            }
            QScrollBar::handle:vertical:hover {
                background: #f0a500;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
            QScrollBar:horizontal {
                background: #111;
                height: 8px;
                border-radius: 4px;
            }
            QScrollBar::handle:horizontal {
                background: #333;
                border-radius: 4px;
                min-width: 30px;
            }
            QScrollBar::handle:horizontal:hover {
                background: #f0a500;
            }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
                width: 0;
            }
            QGroupBox {
                border: 1px solid #2a2a2a;
                border-radius: 4px;
                margin-top: 8px;
                padding-top: 16px;
                color: #e8e8e8;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                color: #f0a500;
                padding: 0 6px;
            }
            QDoubleSpinBox, QSpinBox, QLineEdit, QComboBox {
                background-color: #1a1a1a;
                color: #ddd;
                border: 1px solid #333;
                border-radius: 4px;
                padding: 4px 8px;
            }
            QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus, QComboBox:focus {
                border-color: #f0a500;
            }
            QCheckBox {
                color: #e8e8e8;
            }
            QCheckBox::indicator:checked {
                background-color: #f0a500;
                border: 1px solid #f0a500;
            }
            QPushButton {
                background-color: #222;
                color: #e8e8e8;
                border: 1px solid #333;
                border-radius: 4px;
                padding: 6px 16px;
            }
            QPushButton:hover {
                border-color: #f0a500;
                color: #f0a500;
            }
            QPushButton:pressed {
                background-color: #f0a500;
                color: #0d0d0d;
            }
            QLabel {
                color: #e8e8e8;
            }
        )");

#ifdef _WIN32
        QApplication::setFont(QFont{"Segoe UI", 10});
#else
        QApplication::setFont(QFont{"sans-serif", 10});
#endif
    }
} // namespace forge
