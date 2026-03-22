#pragma once

#include <QComboBox>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QTimer>
#include <QLineEdit>
#include <QListWidget>
#include <QSlider>
#include <QSplitter>
#include <QToolButton>
#include <QTreeWidget>
#include <QWidget>

#include <filesystem>

#include <wax/containers/vector.h>

namespace forge
{
    class AssetBrowserPanel;

    // QTreeWidget that accepts drops and forwards them to the asset browser
    class DropFolderTree : public QTreeWidget
    {
        Q_OBJECT
    public:
        explicit DropFolderTree(AssetBrowserPanel* browser, QWidget* parent = nullptr);

    protected:
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dropEvent(QDropEvent* event) override;

    private:
        AssetBrowserPanel* m_browser{};
    };

    // QListWidget that accepts drops on folder items and forwards them
    class DropContentList : public QListWidget
    {
        Q_OBJECT
    public:
        explicit DropContentList(AssetBrowserPanel* browser, QWidget* parent = nullptr);

    protected:
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseDoubleClickEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dropEvent(QDropEvent* event) override;

    private:
        void StartDrag();

        AssetBrowserPanel* m_browser{};
        QPoint m_dragStartPos;
        bool m_dragPending{false};
        bool m_didDrag{false};
    };

    enum class AssetType : uint8_t
    {
        FOLDER,
        MESH,
        TEXTURE,
        SHADER,
        SCENE,
        MATERIAL,
        AUDIO,
        UNKNOWN
    };

    enum class SortMode : uint8_t
    {
        NAME,
        TYPE,
        DATE
    };

    class ContentItemDelegate;
    class EditorUndoManager;
    class ThumbnailCache;

    class AssetBrowserPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit AssetBrowserPanel(EditorUndoManager* undoManager, QWidget* parent = nullptr);

        void SetAssetsRoot(const char* path);
        void Refresh();
        void MoveAsset(const std::filesystem::path& src, const std::filesystem::path& dstDir);

    signals:
        void assetImported(const QString& path);
        void gltfImportRequested(const QString& path);
        void sceneOpenRequested(const QString& path);
        void assetOpenRequested(const QString& path, AssetType type);
        void assetSelected(const QString& path, AssetType type);

    protected:
        void keyPressEvent(QKeyEvent* event) override;

    private:
        void BuildToolbar();
        void BuildFolderTree();
        void BuildContentArea();
        void BuildStatusBar();

        void NavigateTo(const std::filesystem::path& dir);
        void NavigateBack();
        void NavigateForward();
        void NavigateUp();
        void UpdateBreadcrumb();
        void UpdateStatusBar();
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

        void CopySelected();
        void CutSelected();
        void PasteClipboard();
        void DuplicateSelected();
        void CancelClipboard();
        void MarkCutItems();
        void OnDirectoryChanged(const QString& path);
        void OnFileChanged(const QString& path);
        void WatchCurrentDir();
        void CleanTrash();

        void SetTypeFilter(AssetType type);
        void SetSortMode(SortMode mode);
        void BuildFilterBar();

        void UndoFileAction();
        void RedoFileAction();
        void UpdateIconSize(int size);

        std::filesystem::path TrashPathFor(const std::filesystem::path& path);

        QSplitter* m_splitter{};
        DropFolderTree* m_folderTree{};
        DropContentList* m_contentList{};
        QWidget* m_breadcrumbBar{};
        QWidget* m_statusBar{};
        QLineEdit* m_searchField{};
        QToolButton* m_backBtn{};
        QToolButton* m_forwardBtn{};
        QToolButton* m_upBtn{};
        QToolButton* m_undoBtn{};
        QToolButton* m_redoBtn{};
        QLabel* m_statusLabel{};
        QSlider* m_sizeSlider{};

        std::filesystem::path m_root;
        std::filesystem::path m_currentDir;

        wax::Vector<std::filesystem::path> m_historyBack;
        wax::Vector<std::filesystem::path> m_historyForward;

        EditorUndoManager* m_undoManager{};
        ThumbnailCache* m_thumbnailCache{};

        QFileSystemWatcher m_fsWatcher;
        QTimer m_refreshDebounce;
        QWidget* m_filterBar{};
        QComboBox* m_sortCombo{};

        std::vector<std::filesystem::path> m_clipboard;
        bool m_clipboardIsCut{false};

        AssetType m_typeFilter{AssetType::UNKNOWN};
        SortMode m_sortMode{SortMode::NAME};
        int m_iconSize{80};
    };
} // namespace forge
