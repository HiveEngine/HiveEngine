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

    signals:
        void assetImported(const QString& path);
        void gltfImportRequested(const QString& path);

    private:
        void OnImportClicked();
        void PopulateDirectory(QTreeWidgetItem* parent, const std::filesystem::path& dir);
        std::filesystem::path GetSelectedDirectory() const;

        QTreeWidget* m_tree{};
        std::filesystem::path m_root;
    };
} // namespace forge
