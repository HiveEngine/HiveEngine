#pragma once

#include <QWidget>

#include <string>
#include <vector>

namespace forge
{
    struct DiscoveredProject
    {
        std::string m_name;
        std::string m_version;
        std::string m_path;
        bool m_isCurrentDirectory{false};
    };

    class ProjectHub : public QWidget
    {
        Q_OBJECT

    public:
        explicit ProjectHub(QWidget* parent = nullptr);

        void SetProjects(const std::vector<DiscoveredProject>& projects);

    signals:
        void projectSelected(const QString& path);
        void createProjectRequested(const QString& name, const QString& directory, const QString& version);
        void browseProjectRequested();

    private:
        void BuildUi();
        void BuildProjectCards();

        class QStackedWidget* m_stack{};
        class QWidget* m_projectList{};
        class QVBoxLayout* m_cardLayout{};
        class QLineEdit* m_createName{};
        class QLineEdit* m_createDir{};
        class QLineEdit* m_createVersion{};
        class QLabel* m_statusLabel{};
        std::vector<DiscoveredProject> m_projects;
    };
} // namespace forge
