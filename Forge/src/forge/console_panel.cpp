#include <forge/console_panel.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

namespace forge
{
    namespace
    {
        struct SeverityStyle
        {
            const char* label;
            const char* color;
            const char* badgeBg;
            const char* icon;
        };

        constexpr std::array<SeverityStyle, 4> kStyles = {{
            {"TRC", "#666666", "#2a2a2a", "T"},
            {"INF", "#b0b0b0", "#1a3a1a", "I"},
            {"WRN", "#f0a500", "#3a2a00", "W"},
            {"ERR", "#ff4444", "#3a1a1a", "E"},
        }};

        QString MakeButtonStyle(const char* color, const char* bg, bool active)
        {
            if (active)
            {
                return QString{
                    "QToolButton {"
                    "  background: %1; color: %2; border: none; border-radius: 3px;"
                    "  padding: 2px 10px; font-size: 11px; font-weight: bold; font-family: 'Segoe UI', sans-serif;"
                    "}"
                    "QToolButton:hover { background: %3; }"}
                    .arg(bg, color, QString{bg}.replace("1a", "2a").replace("3a", "4a").replace("2a2a2a", "3a3a3a"));
            }
            return QString{
                "QToolButton {"
                "  background: transparent; color: #444; border: none; border-radius: 3px;"
                "  padding: 2px 10px; font-size: 11px; font-weight: bold; font-family: 'Segoe UI', sans-serif;"
                "}"
                "QToolButton:hover { background: #1a1a1a; color: #666; }"};
        }
    } // namespace

    struct ConsolePanel::LogForwarder
    {
        void Forward(const hive::LogCategory& category, hive::LogSeverity severity, const char* message)
        {
            ConsolePanel* panel = ConsolePanel::s_instance.load(std::memory_order_acquire);
            if (panel != nullptr)
            {
                panel->OnLog(category, severity, message);
            }
        }
    };

    ConsolePanel::LogForwarder ConsolePanel::s_logForwarder{};
    std::atomic<ConsolePanel*> ConsolePanel::s_instance{nullptr};

    ConsolePanel::ConsolePanel(QWidget* parent)
        : QWidget{parent}
    {
        m_entries.reserve(kMaxEntries);
        SetupUi();

        connect(this, &ConsolePanel::logReceived, this, &ConsolePanel::OnLogReceived, Qt::QueuedConnection);

        if (hive::LogManager::IsInitialized())
        {
            s_instance.store(this, std::memory_order_release);
            m_loggerId = hive::LogManager::GetInstance().RegisterLogger(&s_logForwarder, &LogForwarder::Forward);
        }
    }

    ConsolePanel::~ConsolePanel()
    {
        if (hive::LogManager::IsInitialized() && m_loggerId != 0)
        {
            hive::LogManager::GetInstance().UnregisterLogger(m_loggerId);
        }
        if (s_instance.load(std::memory_order_relaxed) == this)
        {
            s_instance.store(nullptr, std::memory_order_release);
        }
    }

    void ConsolePanel::SetupUi()
    {
        auto* root = new QVBoxLayout{this};
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto* toolbar = new QWidget{this};
        toolbar->setFixedHeight(32);
        toolbar->setStyleSheet("QWidget { background: #141414; border-bottom: 1px solid #2a2a2a; }");

        auto* toolLayout = new QHBoxLayout{toolbar};
        toolLayout->setContentsMargins(8, 0, 8, 0);
        toolLayout->setSpacing(4);

        for (size_t i = 0; i < 4; ++i)
        {
            auto* btn = new QToolButton{toolbar};
            btn->setText(QString{"%1  0"}.arg(kStyles[i].label));
            btn->setCheckable(true);
            btn->setChecked(true);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(MakeButtonStyle(kStyles[i].color, kStyles[i].badgeBg, true));
            connect(btn, &QToolButton::toggled, this, &ConsolePanel::OnFilterChanged);
            toolLayout->addWidget(btn);
            m_severityButtons[i] = btn;
        }

        toolLayout->addSpacing(8);

        auto* sep = new QWidget{toolbar};
        sep->setFixedSize(1, 16);
        sep->setStyleSheet("background: #2a2a2a;");
        toolLayout->addWidget(sep);

        toolLayout->addSpacing(4);

        m_categoryButton = new QToolButton{toolbar};
        m_categoryButton->setText("All Categories");
        m_categoryButton->setCursor(Qt::PointingHandCursor);
        m_categoryButton->setPopupMode(QToolButton::InstantPopup);
        m_categoryButton->setStyleSheet(
            "QToolButton {"
            "  background: transparent; color: #888; border: 1px solid #2a2a2a; border-radius: 3px;"
            "  padding: 2px 10px; font-size: 11px; font-family: 'Segoe UI', sans-serif;"
            "}"
            "QToolButton:hover { border-color: #444; color: #b0b0b0; }"
            "QToolButton::menu-indicator { image: none; }");
        m_categoryMenu = new QMenu{m_categoryButton};
        m_categoryMenu->setStyleSheet("QMenu {"
                                      "  background: #1a1a1a; color: #e8e8e8; border: 1px solid #2a2a2a;"
                                      "  padding: 4px 0; font-size: 11px; font-family: 'Segoe UI', sans-serif;"
                                      "}"
                                      "QMenu::item { padding: 4px 20px; }"
                                      "QMenu::item:selected { background: #2a2a2a; }"
                                      "QMenu::indicator { width: 14px; height: 14px; margin-left: 6px; }"
                                      "QMenu::indicator:checked { background: #f0a500; border-radius: 2px; }"
                                      "QMenu::indicator:unchecked { background: #333; border-radius: 2px; }"
                                      "QMenu::separator { height: 1px; background: #2a2a2a; margin: 4px 8px; }");
        m_categoryButton->setMenu(m_categoryMenu);

        auto* showAllAction = m_categoryMenu->addAction("Show All");
        connect(showAllAction, &QAction::triggered, this, [this] {
            m_hiddenCategories.clear();
            for (auto* action : m_categoryMenu->actions())
                if (action->isCheckable())
                    action->setChecked(true);
            m_categoryButton->setText("All Categories");
            RebuildView();
        });
        m_categoryMenu->addSeparator();

        toolLayout->addWidget(m_categoryButton);

        toolLayout->addSpacing(8);

        m_searchBar = new QLineEdit{toolbar};
        m_searchBar->setPlaceholderText("Filter...");
        m_searchBar->setClearButtonEnabled(true);
        m_searchBar->setFixedHeight(22);
        m_searchBar->setStyleSheet("QLineEdit {"
                                   "  background: #0d0d0d; color: #e8e8e8; border: 1px solid #2a2a2a;"
                                   "  border-radius: 3px; padding: 0 8px; font-size: 12px;"
                                   "  font-family: 'Segoe UI', sans-serif; selection-background-color: #f0a500;"
                                   "}"
                                   "QLineEdit:focus { border-color: #f0a500; }");
        connect(m_searchBar, &QLineEdit::textChanged, this, &ConsolePanel::OnSearchChanged);
        toolLayout->addWidget(m_searchBar, 1);

        toolLayout->addSpacing(8);

        m_countLabel = new QLabel{"0", toolbar};
        m_countLabel->setStyleSheet("color: #555; font-size: 11px; font-family: 'Segoe UI', sans-serif;");
        toolLayout->addWidget(m_countLabel);

        toolLayout->addSpacing(8);

        auto* clearBtn = new QToolButton{toolbar};
        clearBtn->setText("Clear");
        clearBtn->setCursor(Qt::PointingHandCursor);
        clearBtn->setStyleSheet("QToolButton {"
                                "  background: transparent; color: #888; border: none; border-radius: 3px;"
                                "  padding: 2px 10px; font-size: 11px; font-family: 'Segoe UI', sans-serif;"
                                "}"
                                "QToolButton:hover { background: #1a1a1a; color: #e8e8e8; }");
        connect(clearBtn, &QToolButton::clicked, this, &ConsolePanel::Clear);
        toolLayout->addWidget(clearBtn);

        root->addWidget(toolbar);

        m_logView = new QTextBrowser{this};
        m_logView->setReadOnly(true);
        m_logView->setOpenLinks(false);
        m_logView->setStyleSheet("QTextBrowser {"
                                 "  background: #0a0a0a; color: #e8e8e8; border: none; padding: 4px 0;"
                                 "  font-family: 'Cascadia Code', 'JetBrains Mono', 'Consolas', monospace;"
                                 "  font-size: 12px; selection-background-color: #f0a500; selection-color: #000;"
                                 "}"
                                 "QScrollBar:vertical {"
                                 "  background: #0a0a0a; width: 8px; margin: 0;"
                                 "}"
                                 "QScrollBar::handle:vertical {"
                                 "  background: #2a2a2a; min-height: 24px; border-radius: 4px;"
                                 "}"
                                 "QScrollBar::handle:vertical:hover { background: #3a3a3a; }"
                                 "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                                 "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");
        m_logView->document()->setDefaultStyleSheet(
            "body { margin: 0; padding: 0; }"
            ".row { padding: 1px 8px; font-size: 12px; white-space: pre; }"
            ".row:hover { background: #141414; }"
            ".time { color: #444; }"
            ".cat  { color: #6a6a6a; }"
            ".sev-trc { color: #666; }"
            ".sev-inf { color: #b0b0b0; }"
            ".sev-wrn { color: #f0a500; }"
            ".sev-err { color: #ff4444; font-weight: bold; }"
            ".badge { border-radius: 2px; padding: 0 4px; font-size: 10px; font-weight: bold; }"
            ".badge-trc { background: #2a2a2a; color: #666; }"
            ".badge-inf { background: #1a3a1a; color: #6abf6a; }"
            ".badge-wrn { background: #3a2a00; color: #f0a500; }"
            ".badge-err { background: #3a1a1a; color: #ff4444; }");

        root->addWidget(m_logView, 1);
    }

    void ConsolePanel::OnLog(const hive::LogCategory& category, hive::LogSeverity severity, const char* message)
    {
        emit logReceived(static_cast<int>(severity), QString::fromUtf8(category.GetFullPath()),
                         QString::fromUtf8(message));
    }

    void ConsolePanel::OnLogReceived(int severity, const QString& category, const QString& message)
    {
        auto sev = static_cast<hive::LogSeverity>(severity);
        auto timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

        if (m_entries.size() >= kMaxEntries)
        {
            m_entries.erase(m_entries.begin(), m_entries.begin() + static_cast<int>(kMaxEntries / 4));
            RebuildView();
        }

        auto sevIdx = static_cast<size_t>(sev);
        if (sevIdx < m_severityCounts.size())
        {
            m_severityCounts[sevIdx]++;
        }

        UpdateCategoryMenu(category);

        LogEntry entry{sev, category, message, timestamp};
        m_entries.push_back(entry);

        if (PassesFilter(entry))
        {
            AppendEntry(entry);
        }

        UpdateButtonLabels();
    }

    void ConsolePanel::UpdateButtonLabels()
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_severityButtons[i]->setText(QString{"%1  %2"}.arg(kStyles[i].label).arg(m_severityCounts[i]));
        }

        uint32_t visible = 0;
        for (const auto& entry : m_entries)
        {
            if (PassesFilter(entry))
                ++visible;
        }
        m_countLabel->setText(QString{"%1 / %2"}.arg(visible).arg(m_entries.size()));
    }

    bool ConsolePanel::PassesFilter(const LogEntry& entry) const
    {
        auto idx = static_cast<size_t>(entry.m_severity);
        if (idx < m_severityEnabled.size() && !m_severityEnabled[idx])
            return false;

        if (!m_hiddenCategories.empty() && m_hiddenCategories.count(entry.m_category) > 0)
            return false;

        if (!m_searchFilter.isEmpty())
        {
            if (!entry.m_message.contains(m_searchFilter, Qt::CaseInsensitive) &&
                !entry.m_category.contains(m_searchFilter, Qt::CaseInsensitive))
            {
                return false;
            }
        }

        return true;
    }

    void ConsolePanel::UpdateCategoryMenu(const QString& category)
    {
        if (m_knownCategories.count(category) > 0)
            return;

        m_knownCategories.insert(category);

        auto* action = m_categoryMenu->addAction(category);
        action->setCheckable(true);
        action->setChecked(true);
        connect(action, &QAction::toggled, this, [this, category](bool checked) {
            if (checked)
                m_hiddenCategories.erase(category);
            else
                m_hiddenCategories.insert(category);

            if (m_hiddenCategories.empty())
                m_categoryButton->setText("All Categories");
            else
                m_categoryButton->setText(QString{"%1/%2"}
                                              .arg(m_knownCategories.size() - m_hiddenCategories.size())
                                              .arg(m_knownCategories.size()));

            RebuildView();
        });
    }

    QString ConsolePanel::FormatEntry(const LogEntry& entry) const
    {
        static constexpr std::array<const char*, 4> kSevClass = {"sev-trc", "sev-inf", "sev-wrn", "sev-err"};
        static constexpr std::array<const char*, 4> kBadgeClass = {"badge-trc", "badge-inf", "badge-wrn", "badge-err"};

        auto idx = static_cast<size_t>(entry.m_severity);
        const char* sevClass = idx < kSevClass.size() ? kSevClass[idx] : "sev-inf";
        const char* badgeClass = idx < kBadgeClass.size() ? kBadgeClass[idx] : "badge-inf";
        const char* sevLabel = idx < kStyles.size() ? kStyles[idx].label : "???";

        auto escaped = entry.m_message.toHtmlEscaped();

        return QString{"<div class=\"row\">"
                       "<span class=\"time\">%1</span>"
                       "  <span class=\"badge %2\">%3</span>"
                       "  <span class=\"cat\">%4</span>"
                       "  <span class=\"%5\">%6</span>"
                       "</div>"}
            .arg(entry.m_timestamp, badgeClass, sevLabel, entry.m_category.toHtmlEscaped(), sevClass, escaped);
    }

    void ConsolePanel::AppendEntry(const LogEntry& entry)
    {
        auto html = FormatEntry(entry);

        auto* scrollBar = m_logView->verticalScrollBar();
        bool wasAtBottom = scrollBar->value() >= scrollBar->maximum() - 4;

        QTextCursor cursor = m_logView->textCursor();
        cursor.movePosition(QTextCursor::End);
        if (!m_logView->document()->isEmpty())
            cursor.insertBlock();
        cursor.insertHtml(html);

        if (m_autoScroll && wasAtBottom)
        {
            scrollBar->setValue(scrollBar->maximum());
        }
    }

    void ConsolePanel::RebuildView()
    {
        m_logView->clear();
        QString html;
        html.reserve(static_cast<int>(m_entries.size()) * 200);
        for (const auto& entry : m_entries)
        {
            if (PassesFilter(entry))
            {
                html += FormatEntry(entry);
            }
        }
        m_logView->setHtml(html);

        if (m_autoScroll)
        {
            auto* scrollBar = m_logView->verticalScrollBar();
            scrollBar->setValue(scrollBar->maximum());
        }

        UpdateButtonLabels();
    }

    void ConsolePanel::Clear()
    {
        m_entries.clear();
        m_severityCounts.fill(0);
        m_knownCategories.clear();
        m_hiddenCategories.clear();
        m_logView->clear();

        while (m_categoryMenu->actions().size() > 2)
            m_categoryMenu->removeAction(m_categoryMenu->actions().last());
        m_categoryButton->setText("All Categories");

        UpdateButtonLabels();
    }

    void ConsolePanel::OnFilterChanged()
    {
        for (size_t i = 0; i < 4; ++i)
        {
            bool checked = m_severityButtons[i]->isChecked();
            m_severityEnabled[i] = checked;
            m_severityButtons[i]->setStyleSheet(MakeButtonStyle(kStyles[i].color, kStyles[i].badgeBg, checked));
        }
        RebuildView();
    }

    void ConsolePanel::OnSearchChanged(const QString& text)
    {
        m_searchFilter = text;
        RebuildView();
    }

} // namespace forge
