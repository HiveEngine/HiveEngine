#include <forge/forge_main_window.h>

#include <hive/core/log.h>

#include <comb/default_allocator.h>

#include <queen/reflect/component_registry.h>
#include <queen/world/world.h>

#include <nectar/mesh/mesh_data.h>
#include <nectar/registry/hiveid_file.h>

#include <waggle/components/mesh_reference.h>
#include <waggle/components/name.h>
#include <waggle/components/transform.h>

#include <forge/asset_browser.h>
#include <forge/console_panel.h>
#include <forge/hierarchy_panel.h>
#include <forge/inspector_panel.h>
#include <forge/project_hub.h>
#include <forge/selection.h>
#include <forge/toolbar.h>
#include <forge/undo.h>
#include <forge/vulkan_viewport_widget.h>

#include <QCloseEvent>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

static const hive::LogCategory LOG_FORGE{"Forge"};

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

    class HexSpinner : public QWidget
    {
    public:
        explicit HexSpinner(QWidget* parent = nullptr)
            : QWidget{parent}
        {
            setFixedSize(48, 48);
            m_timer.setInterval(16);
            connect(&m_timer, &QTimer::timeout, this, [this] {
                m_angle += 3.0;
                update();
            });
            m_timer.start();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter p{this};
            p.setRenderHint(QPainter::Antialiasing);
            p.translate(width() / 2.0, height() / 2.0);
            p.rotate(m_angle);

            const double r = 18.0;
            QPainterPath hex;
            for (int i = 0; i < 6; ++i)
            {
                double a = (60.0 * i - 30.0) * 3.14159265 / 180.0;
                QPointF pt{r * std::cos(a), r * std::sin(a)};
                if (i == 0)
                    hex.moveTo(pt);
                else
                    hex.lineTo(pt);
            }
            hex.closeSubpath();

            QPen pen{QColor{0xf0, 0xa5, 0x00}, 2.5};
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(hex);

            p.setPen(QPen{QColor{0xf0, 0xa5, 0x00, 60}, 2.0});
            p.rotate(30.0);
            p.drawPath(hex);
        }

    private:
        QTimer m_timer;
        double m_angle{0.0};
    };

    void ForgeMainWindow::ShowLoading(const QString& projectName)
    {
        m_hub->hide();
        menuBar()->hide();

        m_loadingWidget = new QWidget{this};
        m_loadingWidget->setStyleSheet("background-color: #0d0d0d;");

        auto* layout = new QVBoxLayout{m_loadingWidget};
        layout->setAlignment(Qt::AlignCenter);
        layout->setSpacing(0);

        auto* spinnerRow = new QHBoxLayout;
        spinnerRow->setAlignment(Qt::AlignCenter);
        spinnerRow->addWidget(new HexSpinner{m_loadingWidget});
        layout->addLayout(spinnerRow);

        layout->addSpacing(24);

        auto* title = new QLabel{"HIVE ENGINE"};
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color: #f0a500; font-size: 24px; font-weight: bold; letter-spacing: 4px;");
        layout->addWidget(title);

        layout->addSpacing(8);

        auto* sub = new QLabel{QString{"Loading %1"}.arg(projectName)};
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet("color: #555; font-size: 12px;");
        layout->addWidget(sub);

        layout->addSpacing(20);

        auto* bar = new QWidget{m_loadingWidget};
        bar->setFixedSize(200, 2);
        bar->setStyleSheet("background-color: #1a1a1a; border-radius: 1px;");

        auto* barGlow = new QWidget{bar};
        barGlow->setFixedSize(60, 2);
        barGlow->setStyleSheet("background-color: #f0a500; border-radius: 1px;");

        auto* barAnim = new QPropertyAnimation{barGlow, "pos", m_loadingWidget};
        barAnim->setDuration(1200);
        barAnim->setStartValue(QPoint{0, 0});
        barAnim->setEndValue(QPoint{140, 0});
        barAnim->setEasingCurve(QEasingCurve::InOutSine);
        barAnim->setLoopCount(-1);
        barAnim->start();

        auto* barRow = new QHBoxLayout;
        barRow->setAlignment(Qt::AlignCenter);
        barRow->addWidget(bar);
        layout->addLayout(barRow);

        setCentralWidget(m_loadingWidget);
        setWindowTitle(QString{"Hive Engine - %1"}.arg(projectName));
    }

    void ForgeMainWindow::ShowEditor()
    {
        if (m_loadingWidget != nullptr)
        {
            m_loadingWidget->hide();
            m_loadingWidget = nullptr;
        }
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

        if (m_assetBrowser)
            m_assetBrowser->Refresh();
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

        m_console = new ConsolePanel{this};
        auto* consoleDock = new QDockWidget{"Console", this};
        consoleDock->setWidget(m_console);
        addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
        tabifyDockWidget(assetDock, consoleDock);
        assetDock->raise();
        m_docks.append(consoleDock);

        m_viewport = new VulkanViewportWidget{this};
        setCentralWidget(m_viewport);
    }

    void ForgeMainWindow::ConnectSignals()
    {
        connect(m_hierarchy, &HierarchyPanel::entitySelected, this, [this](uint32_t) {
            if (m_world && m_registry && m_undo)
                m_inspector->Refresh(*m_world, *m_selection, *m_registry, *m_undo);
        });

        connect(m_hierarchy, &HierarchyPanel::sceneModified, this, [this] { RefreshAll(); });

        connect(m_inspector, &InspectorPanel::sceneModified, this, [this] {
            if (m_world && m_hierarchy)
                m_hierarchy->Refresh(*m_world);
        });

        connect(m_assetBrowser, &AssetBrowserPanel::gltfImportRequested, this, &ForgeMainWindow::gltfImportRequested);

        connect(m_assetBrowser, &AssetBrowserPanel::assetImported, this, [this](const QString& path) {
            std::filesystem::path fsPath{path.toStdString()};
            auto ext = fsPath.extension().string();
            if (ext == ".gltf" || ext == ".glb")
            {
                hive::LogInfo(LOG_FORGE, "Imported: {}", path.toStdString());
            }
            RefreshAll();
        });

        connect(m_hierarchy, &HierarchyPanel::assetDropped, this, [this](const QString& path) {
            if (!m_world)
                return;

            std::filesystem::path fsPath{path.toStdString()};
            auto ext = fsPath.extension().string();

            if (ext == ".nmsh")
            {
                auto meshName = fsPath.stem().string();

                nectar::NmshHeader header{};
                uint32_t submeshCount{0};

                FILE* f{nullptr};
#ifdef _WIN32
                fopen_s(&f, fsPath.string().c_str(), "rb");
#else
                f = fopen(fsPath.string().c_str(), "rb");
#endif
                std::vector<nectar::SubMesh> submeshes;
                if (f)
                {
                    if (std::fread(&header, sizeof(header), 1, f) == 1 && header.m_magic == nectar::kNmshMagic)
                    {
                        submeshCount = header.m_submeshCount;
                        submeshes.resize(submeshCount);
                        std::fread(submeshes.data(), sizeof(nectar::SubMesh), submeshCount, f);
                    }
                    std::fclose(f);
                }

                if (submeshCount <= 1)
                {
                    waggle::MeshReference meshRef{};
                    meshRef.m_meshName = wax::FixedString{meshName.c_str()};
                    meshRef.m_indexCount = 1;

                    (void)m_world->Spawn(waggle::Name{wax::FixedString{meshName.c_str()}}, waggle::Transform{},
                                         waggle::WorldMatrix{}, waggle::TransformVersion{}, std::move(meshRef));
                    hive::LogInfo(LOG_FORGE, "Spawned mesh entity: {}", meshName);
                }
                else
                {
                    queen::Entity root =
                        m_world->Spawn(waggle::Name{wax::FixedString{meshName.c_str()}}, waggle::Transform{},
                                       waggle::WorldMatrix{}, waggle::TransformVersion{});

                    for (uint32_t si = 0; si < submeshCount; ++si)
                    {
                        char primName[128];
                        std::snprintf(primName, sizeof(primName), "%s_prim%u", meshName.c_str(), si);

                        waggle::MeshReference meshRef{};
                        meshRef.m_meshName = wax::FixedString{meshName.c_str()};
                        meshRef.m_meshIndex = static_cast<int32_t>(si);

                        if (si < submeshes.size())
                        {
                            meshRef.m_indexCount = submeshes[si].m_indexCount;
                            meshRef.m_materialIndex = submeshes[si].m_materialIndex;
                            char matName[32];
                            std::snprintf(matName, sizeof(matName), "Material_%d", submeshes[si].m_materialIndex);
                            meshRef.m_materialName = wax::FixedString{matName};
                        }

                        queen::Entity child =
                            m_world->Spawn(waggle::Name{wax::FixedString{primName}}, waggle::Transform{},
                                           waggle::WorldMatrix{}, waggle::TransformVersion{}, std::move(meshRef));

                        m_world->SetParent(child, root);
                    }

                    hive::LogInfo(LOG_FORGE, "Spawned mesh hierarchy: {} ({} primitives)", meshName, submeshCount);
                }

                RefreshAll();
                emit m_hierarchy->sceneModified();
            }
            else if (ext == ".hscene")
            {
                hive::LogInfo(LOG_FORGE, "Scene drop not yet implemented: {}", fsPath.string());
            }
            else
            {
                hive::LogWarning(LOG_FORGE, "Unsupported asset drop: {}", path.toStdString());
            }
        });
    }
} // namespace forge
