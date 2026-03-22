#pragma once

#include <QMainWindow>

#include <cstddef>

namespace terra
{
    struct WindowContext;
}

namespace swarm
{
    struct RenderContext;
}

namespace queen
{
    class World;
    template <size_t> class ComponentRegistry;
} // namespace queen

namespace forge
{
    class AssetBrowserPanel;
    class ConsolePanel;
    class EditorSelection;
    class EditorToolbar;
    class HierarchyPanel;
    class InspectorPanel;
    class ProgressOverlay;
    class ProjectHub;
    class UndoStack;
    class VulkanViewportWidget;

    class ForgeMainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit ForgeMainWindow(QWidget* parent = nullptr);

        void Initialize(queen::World* world, EditorSelection* selection, const queen::ComponentRegistry<256>* registry,
                        UndoStack* undo);
        void ShowHub(const std::vector<struct DiscoveredProject>& projects);
        void ShowLoading(const QString& projectName);
        void ShowEditor();

        void RefreshAll();
        void SetAssetsRoot(const char* path);
        void SetSceneDirty(bool dirty);
        void SetSceneName(const QString& name);
        void AttachViewport(terra::WindowContext* window, swarm::RenderContext* ctx);
        void RenderFrame();

        void ShowProgress(const QString& title);
        void ProgressSetStep(const QString& step);
        void ProgressSetProgress(int current, int total);
        void HideProgress();

        EditorToolbar* Toolbar();

    protected:
        void closeEvent(QCloseEvent* event) override;

    signals:
        void saveRequested();
        void saveAsRequested();
        void openRequested();
        void hubProjectSelected(const QString& path);
        void hubCreateRequested(const QString& name, const QString& dir, const QString& version);
        void hubBrowseRequested();
        void editorCloseRequested();
        void gltfImportRequested(const QString& path);
        void sceneOpenRequested(const QString& path);
        void sceneModified();

    private:
        void CreateMenus();
        void CreateDocks();
        void ConnectSignals();

        queen::World* m_world{};
        EditorSelection* m_selection{};
        const queen::ComponentRegistry<256>* m_registry{};
        UndoStack* m_undo{};

        HierarchyPanel* m_hierarchy{};
        InspectorPanel* m_inspector{};
        AssetBrowserPanel* m_assetBrowser{};
        ConsolePanel* m_console{};
        VulkanViewportWidget* m_viewport{};
        EditorToolbar* m_toolbar{};
        ProjectHub* m_hub{};
        QWidget* m_loadingWidget{};
        QWidget* m_editorWidget{};
        ProgressOverlay* m_progressOverlay{};
        QList<class QDockWidget*> m_docks;
        QString m_sceneName;
        bool m_sceneDirty{false};
    };
} // namespace forge
