#pragma once

#include <hive/core/log.h>

#include <QWidget>

#include <array>
#include <mutex>
#include <set>
#include <vector>

class QCheckBox;
class QLabel;
class QLineEdit;
class QMenu;
class QScrollBar;
class QTextBrowser;
class QToolButton;

namespace forge
{

    class ConsolePanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit ConsolePanel(QWidget* parent = nullptr);
        ~ConsolePanel() override;

        void Clear();

    signals:
        void logReceived(int severity, const QString& category, const QString& message);

    private slots:
        void OnLogReceived(int severity, const QString& category, const QString& message);
        void OnFilterChanged();
        void OnSearchChanged(const QString& text);

    private:
        struct LogEntry
        {
            hive::LogSeverity m_severity;
            QString m_category;
            QString m_message;
            QString m_timestamp;
        };

        void SetupUi();
        void AppendEntry(const LogEntry& entry);
        void RebuildView();
        void UpdateButtonLabels();
        bool PassesFilter(const LogEntry& entry) const;
        QString FormatEntry(const LogEntry& entry) const;

        void OnLog(const hive::LogCategory& category, hive::LogSeverity severity, const char* message);

        struct LogForwarder;
        static LogForwarder s_logForwarder;
        static ConsolePanel* s_instance;

        static constexpr size_t kMaxEntries = 8192;

        QTextBrowser* m_logView{};
        QLineEdit* m_searchBar{};
        QLabel* m_countLabel{};
        std::array<QToolButton*, 4> m_severityButtons{};
        std::array<bool, 4> m_severityEnabled{true, true, true, true};
        std::array<uint32_t, 4> m_severityCounts{};

        void UpdateCategoryMenu(const QString& category);

        std::vector<LogEntry> m_entries;
        QString m_searchFilter;
        bool m_autoScroll{true};

        QToolButton* m_categoryButton{};
        QMenu* m_categoryMenu{};
        std::set<QString> m_knownCategories;
        std::set<QString> m_hiddenCategories;

        hive::LogManager::LoggerId m_loggerId{};
        std::mutex m_pendingMutex;
    };

} // namespace forge
