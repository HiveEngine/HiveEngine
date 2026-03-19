#pragma once

#include <QTreeWidget>
#include <QWidget>

#include <filesystem>

namespace forge
{
    class AssetBrowserPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit AssetBrowserPanel(QWidget* parent = nullptr);

        void SetAssetsRoot(const char* path);

        void Refresh();

    private:
        void PopulateDirectory(QTreeWidgetItem* parent, const std::filesystem::path& dir);

        QTreeWidget* m_tree{};
        std::filesystem::path m_root;
    };
} // namespace forge
