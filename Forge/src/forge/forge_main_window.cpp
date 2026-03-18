#include <forge/forge_main_window.h>

#include <forge/asset_browser.h>
#include <forge/hierarchy_panel.h>
#include <forge/inspector_panel.h>
#include <forge/project_hub.h>
#include <forge/selection.h>
#include <forge/toolbar.h>
#include <forge/undo.h>
#include <forge/vulkan_viewport_widget.h>
#include <queen/reflect/component_registry.h>

#include <QCloseEvent>
#include <QDockWidget>
#include <QMenuBar>
#include <QStackedWidget>

namespace forge
{
    ForgeMainWindow::ForgeMainWindow(QWidget* parent)
        : QMainWindow{parent}
    {
        setWindowTitle("Hive Engine");
        resize(1200, 750);

        m_hub = new ProjectHub{this};
        setCentralWidget(m_hub);

        connect(m_hub, &ProjectHub::projectSelected, this, &ForgeMainWindow::hubProjectSelected);
        connect(m_hub, &ProjectHub::createProjectRequested, this, &ForgeMainWindow::hubCreateRequested);
        connect(m_hub, &ProjectHub::browseProjectRequested, this, &ForgeMainWindow::hubBrowseRequested);
    }

    void ForgeMainWindow::Initialize(queen::World* world, EditorSelection* selection,
                                     const queen::ComponentRegistry<256>* registry, UndoStack* undo)
    {
        m_world = world;
        m_selection = selection;
        m_registry = registry;
        m_undo = undo;
    }

    void ForgeMainWindow::ShowHub(const std::vector<DiscoveredProject>& projects)
    {
        menuBar()->hide();
        for (auto* dock : m_docks)
            dock->hide();

        m_hub->SetProjects(projects);
        m_hub->show();
        setCentralWidget(m_hub);
        setWindowTitle("Hive Engine");
        resize(1200, 750);
    }

    void ForgeMainWindow::ShowEditor()
    {
        m_hub->hide();

        CreateMenus();
        CreateDocks();
        ConnectSignals();
        menuBar()->show();

        setWindowTitle("Forge Editor");
        resize(1600, 900);
    }

    void ForgeMainWindow::closeEvent(QCloseEvent* event)
    {
        emit editorCloseRequested();
        event->accept();
    }

    void ForgeMainWindow::RefreshAll()
    {
        if (m_world && m_hierarchy)
            m_hierarchy->Refresh(*m_world);

        if (m_world && m_inspector && m_selection && m_registry && m_undo)
            m_inspector->Refresh(*m_world, *m_selection, *m_registry, *m_undo);
    }

    void ForgeMainWindow::SetAssetsRoot(const char* path)
    {
        if (m_assetBrowser)
            m_assetBrowser->SetAssetsRoot(path);
    }

    EditorToolbar* ForgeMainWindow::Toolbar()
    {
        return m_toolbar;
    }

    void ForgeMainWindow::AttachViewport(terra::WindowContext* window, swarm::RenderContext* ctx)
    {
        if (m_viewport)
        {
            m_viewport->EmbedGlfwWindow(window);
            m_viewport->SetRenderContext(ctx);
        }
    }

    void ForgeMainWindow::RenderFrame()
    {
        if (m_viewport)
            m_viewport->RenderFrame();
    }

    void ForgeMainWindow::CreateMenus()
    {
        menuBar()->clear();

        auto* fileMenu = menuBar()->addMenu("&File");

        auto* newAction = fileMenu->addAction("&New Scene");
        Q_UNUSED(newAction);

        auto* openAction = fileMenu->addAction("&Open Scene");
        openAction->setShortcut(QKeySequence{"Ctrl+O"});
        connect(openAction, &QAction::triggered, this, &ForgeMainWindow::openRequested);

        fileMenu->addSeparator();

        auto* saveAction = fileMenu->addAction("&Save Scene");
        saveAction->setShortcut(QKeySequence{"Ctrl+S"});
        connect(saveAction, &QAction::triggered, this, &ForgeMainWindow::saveRequested);

        auto* saveAsAction = fileMenu->addAction("Save Scene &As...");
        saveAsAction->setShortcut(QKeySequence{"Ctrl+Shift+S"});
        connect(saveAsAction, &QAction::triggered, this, &ForgeMainWindow::saveAsRequested);

        fileMenu->addSeparator();

        auto* quitAction = fileMenu->addAction("&Quit");
        quitAction->setShortcut(QKeySequence{"Alt+F4"});
        connect(quitAction, &QAction::triggered, this, &QMainWindow::close);

        auto* editMenu = menuBar()->addMenu("&Edit");

        auto* undoAction = editMenu->addAction("&Undo");
        undoAction->setShortcut(QKeySequence{"Ctrl+Z"});
        connect(undoAction, &QAction::triggered, this, [this] {
            if (m_undo && m_world)
            {
                m_undo->Undo(*m_world);
                RefreshAll();
            }
        });

        auto* redoAction = editMenu->addAction("&Redo");
        redoAction->setShortcuts({QKeySequence{"Ctrl+Y"}, QKeySequence{"Ctrl+Shift+Z"}});
        connect(redoAction, &QAction::triggered, this, [this] {
            if (m_undo && m_world)
            {
                m_undo->Redo(*m_world);
                RefreshAll();
            }
        });
    }

    void ForgeMainWindow::CreateDocks()
    {
        m_hierarchy = new HierarchyPanel{*m_selection, this};
        auto* hierarchyDock = new QDockWidget{"Hierarchy", this};
        hierarchyDock->setWidget(m_hierarchy);
        addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);
        m_docks.append(hierarchyDock);

        m_inspector = new InspectorPanel{this};
        auto* inspectorDock = new QDockWidget{"Inspector", this};
        inspectorDock->setWidget(m_inspector);
        inspectorDock->setMinimumWidth(300);
        addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
        m_docks.append(inspectorDock);

        m_assetBrowser = new AssetBrowserPanel{this};
        auto* assetDock = new QDockWidget{"Assets", this};
        assetDock->setWidget(m_assetBrowser);
        addDockWidget(Qt::BottomDockWidgetArea, assetDock);
        m_docks.append(assetDock);

        m_viewport = new VulkanViewportWidget{this};
        setCentralWidget(m_viewport);
    }

    void ForgeMainWindow::ConnectSignals()
    {
        connect(m_hierarchy, &HierarchyPanel::entitySelected, this, [this](uint32_t) {
            if (m_world && m_registry && m_undo)
                m_inspector->Refresh(*m_world, *m_selection, *m_registry, *m_undo);
        });

        connect(m_hierarchy, &HierarchyPanel::sceneModified, this, [this] {
            RefreshAll();
        });

        connect(m_inspector, &InspectorPanel::sceneModified, this, [this] {
            if (m_world && m_hierarchy)
                m_hierarchy->Refresh(*m_world);
        });
    }
} // namespace forge
