#include <hive/hive_config.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <nectar/import/gltf_import_job.h>

#include <waggle/app_context.h>
#include <waggle/engine_runner.h>
#include <waggle/project/gameplay_module.h>
#include <waggle/project/project_context.h>
#include <waggle/project/project_manager.h>

#include <swarm/swarm.h>

#include <brood/launcher/launcher_cli.h>
#include <brood/launcher/launcher_types.h>
#include <launcher/launcher_platform.h>
#include <launcher/launcher_project.h>
#include <launcher/launcher_scene.h>

#if HIVE_MODE_EDITOR
#include <forge/forge_main_window.h>
#include <forge/project_hub.h>
#include <forge/theme.h>

#include <QApplication>
#endif

#include <brood/process_runtime.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace
{

    using namespace brood::launcher;

    class LauncherProcessSession
    {
    public:
        int Run(int argc, char* argv[])
        {
            LauncherCommandLine commandLine{};
            if (!TryParseCommandLine(argc, argv, &commandLine))
            {
                return 1;
            }

            return Run(commandLine);
        }

    private:
        int Run(const LauncherCommandLine& commandLine)
        {
            LauncherState state{};
            state.m_exitAfterSetup = commandLine.m_exitAfterSetup;

            waggle::EngineMode requestedMode{};
            if (!ResolveLauncherMode(commandLine, &requestedMode))
            {
                PrintUsage();
                return 1;
            }

            std::error_code ec;
            std::filesystem::path startupProjectPath = commandLine.m_projectPath;
            bool showProjectHub = false;

#if HIVE_MODE_EDITOR
            if (startupProjectPath.empty() && requestedMode == waggle::EngineMode::EDITOR)
            {
                showProjectHub = true;
                InitializeProjectHub(state.m_hub);
            }
#endif

            if (!showProjectHub)
            {
                if (startupProjectPath.empty())
                {
                    startupProjectPath = std::filesystem::current_path(ec) / "project.hive";
                }

                if (!ResolveProjectFilePath(startupProjectPath, &startupProjectPath))
                {
                    std::fprintf(stderr, "Error: project file not found: %s\n", startupProjectPath.string().c_str());
                    PrintUsage();
                    return 1;
                }

                state.m_projectPath = wax::String{startupProjectPath.generic_string().c_str()};
            }

            const wax::String windowTitle = state.m_projectPath.IsEmpty() ? wax::String{"HiveEngine Launcher"}
                                                                          : BuildWindowTitle(state.m_projectPath);

            waggle::EngineConfig config{};
            config.m_windowTitle = windowTitle.CStr();
#if HIVE_MODE_EDITOR
            config.m_windowWidth = 1920;
            config.m_windowHeight = 1080;
            config.m_deferWindow = true;
#endif
            config.m_mode = requestedMode;

            waggle::EngineCallbacks callbacks{};
            callbacks.m_onSetup = [](waggle::EngineContext& ctx, void* ud) -> bool {
                auto& s = *static_cast<LauncherState*>(ud);
                auto& alloc = s.m_alloc.Get();

                s.m_project = comb::New<waggle::ProjectManager>(alloc, alloc);
                ctx.m_world->InsertResource(waggle::AppContext{ctx.m_app});

#if HIVE_MODE_EDITOR
                RegisterSceneComponentTypes(s);
#endif

#if HIVE_MODE_EDITOR
                s.m_mainWindow = new forge::ForgeMainWindow;
                s.m_mainWindow->Initialize(ctx.m_world, &s.m_selection, &s.m_componentRegistry, s.m_undo.get());

                InitializeProjectHub(s.m_hub);
                std::vector<forge::DiscoveredProject> hubProjects;
                for (const auto& p : s.m_hub.m_discoveredProjects)
                    hubProjects.push_back({std::string{p.m_name.CStr()}, std::string{p.m_version.CStr()},
                                           std::string{p.m_path.CStr()}, p.m_isCurrentDirectory});

                auto transitionToEditor = [&ctx, &s](const QString& projectName) {
                    s.m_mainWindow->ShowLoading(projectName);
                    QApplication::processEvents();

                    waggle::CreateWindowAndRenderer(ctx, s.m_windowTitle.CStr(), s.m_windowWidth, s.m_windowHeight);

                    s.m_mainWindow->ShowEditor();
                    if (ctx.m_renderContext != nullptr && ctx.m_window != nullptr)
                        s.m_mainWindow->AttachViewport(ctx.m_window, ctx.m_renderContext);
                    s.m_mainWindow->RefreshAll();
                    if (!s.m_assetsRoot.IsEmpty())
                        s.m_mainWindow->SetAssetsRoot(s.m_assetsRoot.CStr());
                };

                if (s.m_projectPath.IsEmpty())
                {
                    s.m_mainWindow->ShowHub(hubProjects);
                }
                else
                {
                    transitionToEditor(QString::fromUtf8(s.m_projectPath.CStr()));
                }

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::hubProjectSelected,
                                 [&ctx, &s, transitionToEditor](const QString& path) {
                                     if (OpenProject(ctx, s, std::filesystem::path{path.toStdString()}))
                                         transitionToEditor(QString::fromUtf8(s.m_projectPath.CStr()));
                                 });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::editorCloseRequested,
                                 [&ctx]() { ctx.m_app->RequestStop(); });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::saveRequested,
                                 [&ctx, &s]() { SaveEditorScene(ctx, s); });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::saveAsRequested,
                                 [&ctx, &s]() { SaveEditorSceneAs(ctx, s); });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::openRequested,
                                 [&ctx, &s]() { OpenEditorSceneWithPicker(ctx, s); });

                QObject::connect(
                    s.m_mainWindow, &forge::ForgeMainWindow::hubCreateRequested,
                    [&ctx, &s, transitionToEditor](const QString& name, const QString& dir, const QString& version) {
                        auto& hub = s.m_hub;
                        CopyStringToBuffer(wax::String{name.toStdString().c_str()}, hub.m_createName.Data(),
                                           hub.m_createName.Size());
                        CopyStringToBuffer(wax::String{dir.toStdString().c_str()}, hub.m_createDirectory.Data(),
                                           hub.m_createDirectory.Size());
                        CopyStringToBuffer(wax::String{version.toStdString().c_str()}, hub.m_createVersion.Data(),
                                           hub.m_createVersion.Size());
                        if (CreateProjectFromHub(ctx, s))
                            transitionToEditor(QString::fromUtf8(s.m_projectPath.CStr()));
                    });

                QObject::connect(
                    s.m_mainWindow, &forge::ForgeMainWindow::hubBrowseRequested, [&ctx, &s, transitionToEditor]() {
                        std::filesystem::path selectedPath{};
                        const auto result = ShowNativePathPicker(NativePathPickerMode::PROJECT_FILE, {}, &selectedPath);
                        if (result == NativePathPickerResult::SUCCESS && OpenProject(ctx, s, selectedPath))
                            transitionToEditor(QString::fromUtf8(s.m_projectPath.CStr()));
                    });

                QObject::connect(
                    s.m_mainWindow, &forge::ForgeMainWindow::gltfImportRequested, [&ctx, &s](const QString& path) {
                        hive::LogInfo(LOG_LAUNCHER, "gltfImportRequested received: {}", path.toStdString());

                        if (s.m_project == nullptr)
                            return;

                        auto& alloc = s.m_alloc.Get();
                        auto& db = s.m_project->Database();
                        std::string gltfStdPath = path.toStdString();

                        nectar::GltfImportDesc desc{};
                        desc.m_gltfPath = wax::StringView{gltfStdPath.c_str()};
                        desc.m_assetsDir = s.m_project->Paths().m_assets.View();

                        auto result = nectar::ExecuteGltfImport(desc, db, alloc, &s.m_project->Import());

                        if (result.m_success)
                        {
                            hive::LogInfo(LOG_LAUNCHER, "glTF import: {} textures, {} materials, {} meshes",
                                          result.m_textureCount, result.m_materialCount, result.m_meshCount);
                        }
                        else
                        {
                            hive::LogError(LOG_LAUNCHER, "glTF import failed: {}", result.m_error.CStr());
                        }

                        s.m_mainWindow->RefreshAll();
                    });

                s.m_mainWindow->show();
#endif

                if (!s.m_projectPath.IsEmpty() && !OpenProject(ctx, s, std::filesystem::path{s.m_projectPath.CStr()}))
                {
                    comb::Delete(alloc, s.m_project);
                    s.m_project = nullptr;
                    return false;
                }

#if HIVE_MODE_EDITOR
                if (s.m_mainWindow && !s.m_assetsRoot.IsEmpty())
                    s.m_mainWindow->SetAssetsRoot(s.m_assetsRoot.CStr());
#endif

                if (s.m_exitAfterSetup)
                {
                    ctx.m_app->RequestStop();
                }

                return true;
            };

            callbacks.m_onFrame = [](waggle::EngineContext& ctx, void* ud) {
                auto& s = *static_cast<LauncherState*>(ud);
                if (s.m_project != nullptr)
                {
                    s.m_project->Update();
                }

#if HIVE_MODE_EDITOR
                if (s.m_mainWindow != nullptr)
                {
                    if (s.m_project != nullptr && s.m_project->LastReloadCount() > 0)
                        s.m_mainWindow->RefreshAll();

                    s.m_mainWindow->RenderFrame();
                    QApplication::processEvents();
                }
#elif HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                if (ctx.m_renderContext != nullptr)
                {
                    swarm::DrawPipeline(ctx.m_renderContext);
                }
#endif
            };

            callbacks.m_onShutdown = [](waggle::EngineContext& ctx, void* ud) {
                auto& s = *static_cast<LauncherState*>(ud);

#if HIVE_MODE_EDITOR
                delete s.m_mainWindow;
                s.m_mainWindow = nullptr;
#endif

                if (s.m_gameplay.IsRegistered())
                {
                    s.m_gameplay.Unregister(*ctx.m_world);
                }

                if (s.m_projectOpen && s.m_project != nullptr)
                {
                    ctx.m_world->RemoveResource<waggle::ProjectContext>();
                    s.m_project->Close();
                    s.m_projectOpen = false;
                }

                if (s.m_project != nullptr)
                {
                    comb::Delete(s.m_alloc.Get(), s.m_project);
                    s.m_project = nullptr;
                }

                // Do not FreeLibrary() during launcher teardown. The process exits
                // immediately after Run() returns, so leaving the module loaded lets
                // the OS reclaim it and avoids shutdown stalls in project DLL
                // destructors while the runtime is already coming down.
                s.m_gameplay.Abandon();
            };

            callbacks.m_userData = &state;
            return waggle::Run(config, callbacks);
        }
    };

} // anonymous namespace

int main(int argc, char* argv[])
{
    brood::ProcessRuntime runtime{};
#if HIVE_MODE_EDITOR
    QApplication qtApp{argc, argv};
    forge::ApplyForgeTheme();
#endif
    int result = 1;
    {
        LauncherProcessSession session{};
        result = session.Run(argc, argv);
    }
    runtime.Exit(result);
}
