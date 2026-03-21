#pragma once

#include <QLineEdit>
#include <QListWidget>
#include <QSplitter>
#include <QToolButton>
#include <QTreeWidget>
#include <QWidget>

#include <filesystem>

#include <wax/containers/vector.h>

namespace forge
{
    enum class AssetType : uint8_t
    {
        Folder,
        Mesh,
        Texture,
        Shader,
        Scene,
        Material,
        Audio,
        Unknown
    };

    AssetType ClassifyExtension(const std::string& ext);

    class ContentItemDelegate;

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
        void BuildToolbar();
        void BuildFolderTree();
        void BuildContentArea();

        void NavigateTo(const std::filesystem::path& dir);
        void NavigateBack();
        void NavigateForward();
        void NavigateUp();
        void UpdateBreadcrumb();
        void PopulateFolderTree(QTreeWidgetItem* parent, const std::filesystem::path& dir);
        void PopulateContent();
        void ApplySearchFilter();
        void SyncTreeSelection();

        void OnFolderTreeClicked(QTreeWidgetItem* item);
        void OnContentDoubleClicked(QListWidgetItem* item);
        void OnContentContextMenu(const QPoint& pos);
        void OnFolderTreeContextMenu(const QPoint& pos);
        void OnImportClicked();
        void OnNewFolderClicked();

        void RenamePath(const std::filesystem::path& fsPath);
        void DeletePath(const std::filesystem::path& fsPath);
        void RenameSelected();
        void DeleteSelected();
        void ShowInExplorer(const std::filesystem::path& path);
        void MoveAsset(const std::filesystem::path& src, const std::filesystem::path& dstDir);

        QSplitter* m_splitter{};
        QTreeWidget* m_folderTree{};
        QListWidget* m_contentList{};
        QWidget* m_breadcrumbBar{};
        QLineEdit* m_searchField{};
        QToolButton* m_backBtn{};
        QToolButton* m_forwardBtn{};
        QToolButton* m_upBtn{};

        std::filesystem::path m_root;
        std::filesystem::path m_currentDir;

        wax::Vector<std::filesystem::path> m_historyBack;
        wax::Vector<std::filesystem::path> m_historyForward;

        int m_iconSize{80};
    };
} // namespace forge
