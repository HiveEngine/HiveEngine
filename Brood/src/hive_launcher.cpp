#include <hive/hive_config.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <nectar/import/gltf_import_job.h>
#include <nectar/pipeline/hot_reload.h>

#include <waggle/app_context.h>
#include <waggle/disabled_propagation.h>
#include <waggle/systems/transform_system.h>
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
#include <QMessageBox>
#include <QProcess>
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

            if (requestedMode != waggle::EngineMode::HEADLESS && !state.m_exitAfterSetup)
            {
                state.m_jobAlloc = std::make_unique<comb::BuddyAllocator>(4 * 1024 * 1024);
                state.m_jobSystem.reset(
                    new drone::JobSystem<comb::BuddyAllocator>{*state.m_jobAlloc});
                state.m_jobSystem->Start();
            }

            waggle::EngineConfig config{};
            if (state.m_jobSystem)
                config.m_jobs = drone::MakeJobSubmitter(*state.m_jobSystem);
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
                waggle::RegisterDisabledObservers(*ctx.m_world);
                waggle::RegisterTransformObservers(*ctx.m_world);
                waggle::RegisterEngineSystems(*ctx.m_world);

#if HIVE_MODE_EDITOR
                RegisterSceneComponentTypes(s);
#endif

#if HIVE_MODE_EDITOR
                s.m_mainWindow = new forge::ForgeMainWindow;
                s.m_mainWindow->Initialize(ctx.m_world, &s.m_selection, &s.m_componentRegistry);

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

                    if (!s.m_currentScenePath.IsEmpty())
                    {
                        auto stem = std::filesystem::path{s.m_currentScenePath.CStr()}.stem().string();
                        s.m_mainWindow->SetSceneName(QString::fromStdString(stem));
                    }
                    s.m_mainWindow->SetSceneDirty(false);
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

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::saveRequested, [&ctx, &s]() {
                    SaveEditorScene(ctx, s);
                    s.m_mainWindow->SetSceneDirty(s.m_sceneDirty);
                });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::saveAsRequested, [&ctx, &s]() {
                    SaveEditorSceneAs(ctx, s);
                    s.m_mainWindow->SetSceneDirty(s.m_sceneDirty);
                });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::openRequested, [&ctx, &s]() {
                    if (s.m_sceneDirty)
                    {
                        auto answer = QMessageBox::question(
                            s.m_mainWindow, "Unsaved Changes",
                            "The current scene has unsaved changes. Do you want to save before opening?",
                            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

                        if (answer == QMessageBox::Cancel)
                        {
                            return;
                        }
                        if (answer == QMessageBox::Save)
                        {
                            SaveEditorScene(ctx, s);
                        }
                    }
                    OpenEditorSceneWithPicker(ctx, s);
                    s.m_mainWindow->SetSceneDirty(s.m_sceneDirty);
                });

                QObject::connect(
                    s.m_mainWindow, &forge::ForgeMainWindow::sceneOpenRequested,
                    [&ctx, &s](const QString& path) {
                        if (s.m_sceneDirty)
                        {
                            auto answer = QMessageBox::question(
                                s.m_mainWindow, "Unsaved Changes",
                                "The current scene has unsaved changes. Do you want to save before opening?",
                                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

                            if (answer == QMessageBox::Cancel)
                            {
                                return;
                            }
                            if (answer == QMessageBox::Save)
                            {
                                SaveEditorScene(ctx, s);
                            }
                        }

                        s.m_pendingEditorAction = PendingEditorAction::NONE;
                        auto fsPath = std::filesystem::path{path.toStdString()};
                        LoadEditorSceneAsset(ctx, s, fsPath);
                        s.m_mainWindow->RefreshAll();
                        s.m_mainWindow->SetSceneName(QString::fromStdString(fsPath.stem().string()));
                        s.m_mainWindow->SetSceneDirty(false);
                    });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::sceneModified, [&s]() {
                    s.m_sceneDirty = true;
                    s.m_mainWindow->SetSceneDirty(true);
                });

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
                    s.m_mainWindow, &forge::ForgeMainWindow::gltfImportRequested, [&s](const QString& path) {
                        hive::LogInfo(LOG_LAUNCHER, "gltfImportRequested received: {}", path.toStdString());

                        if (s.m_project == nullptr)
                            return;

                        auto gltfName = std::filesystem::path{path.toStdString()}.stem().string();
                        s.m_mainWindow->ShowProgress(
                            QString{"Importing %1..."}.arg(QString::fromStdString(gltfName)));

                        auto& alloc = s.m_alloc.Get();
                        auto& db = s.m_project->Database();
                        std::string gltfStdPath = path.toStdString();

                        nectar::GltfImportDesc desc{};
                        desc.m_gltfPath = wax::StringView{gltfStdPath.c_str()};
                        desc.m_assetsDir = s.m_project->Paths().m_assets.View();

                        auto progressCb = [](const char* step, uint32_t current, uint32_t total, void* ud) {
                            auto* mw = static_cast<forge::ForgeMainWindow*>(ud);
                            mw->ProgressSetStep(QString::fromUtf8(step));
                            mw->ProgressSetProgress(static_cast<int>(current), static_cast<int>(total));
                            QApplication::processEvents();
                        };

                        auto result = nectar::ExecuteGltfImport(desc, db, alloc, &s.m_project->Import(), progressCb,
                                                                s.m_mainWindow);

                        s.m_mainWindow->HideProgress();

                        if (result.m_success)
                        {
                            hive::LogInfo(LOG_LAUNCHER, "glTF import: {} textures, {} materials, {} meshes",
                                          result.m_textureCount, result.m_materialCount, result.m_meshCount);
                        }
                        else
                        {
                            hive::LogError(LOG_LAUNCHER, "glTF import failed: {}", result.m_error.CStr());
                        }

                        if (auto* hr = s.m_project->HotReload())
                            hr->DrainPending();

                        s.m_mainWindow->RefreshAll();
                    });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::buildRequested, [&s]() {
                    if (s.m_buildProcess != nullptr && s.m_buildProcess->state() != QProcess::NotRunning)
                    {
                        hive::LogWarning(LOG_LAUNCHER, "Build already in progress");
                        return;
                    }

                    auto exePath = std::filesystem::path{QCoreApplication::applicationFilePath().toStdString()};
                    auto buildDir = exePath.parent_path().parent_path().parent_path();

                    hive::LogInfo(LOG_LAUNCHER, "Building gameplay module...");

                    auto* process = new QProcess{s.m_mainWindow};
                    s.m_buildProcess = process;

                    QObject::connect(process,
                                     qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                                     [&s, process](int exitCode, QProcess::ExitStatus) {
                                         if (exitCode == 0)
                                         {
                                             hive::LogInfo(LOG_LAUNCHER, "Build succeeded");
                                             s.m_pendingDllReload = true;
                                         }
                                         else
                                         {
                                             hive::LogError(LOG_LAUNCHER, "Build failed (exit code {})", exitCode);
                                         }
                                         s.m_buildProcess = nullptr;
                                         process->deleteLater();
                                     });

                    QObject::connect(process, &QProcess::readyReadStandardOutput, [process]() {
                        auto data = process->readAllStandardOutput();
                        for (const auto& line : data.split('\n'))
                        {
                            auto trimmed = line.trimmed();
                            if (!trimmed.isEmpty())
                                hive::LogInfo(LOG_LAUNCHER, "[build] {}", trimmed.toStdString());
                        }
                    });

                    QObject::connect(process, &QProcess::readyReadStandardError, [process]() {
                        auto data = process->readAllStandardError();
                        for (const auto& line : data.split('\n'))
                        {
                            auto trimmed = line.trimmed();
                            if (!trimmed.isEmpty())
                                hive::LogError(LOG_LAUNCHER, "[build] {}", trimmed.toStdString());
                        }
                    });

                    auto buildDirStr = QString::fromStdString(buildDir.generic_string());
#if HIVE_PLATFORM_WINDOWS
                    auto target = QStringLiteral("gameplay.dll");
#else
                    auto target = QStringLiteral("gameplay.so");
#endif
                    process->setWorkingDirectory(buildDirStr);
                    process->start("cmake", {"--build", buildDirStr, "--target", target});
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

                if (s.m_pendingDllReload && ctx.m_world != nullptr)
                {
                    s.m_pendingDllReload = false;
                    if (s.m_gameplay.IsLoaded())
                    {
                        s.m_gameplay.Unregister(*ctx.m_world);
                        ctx.m_world->ClearSystems();
                        s.m_gameplay.Unload();
                    }
                    waggle::RegisterEngineSystems(*ctx.m_world);
                    TryLoadGameplayModule(ctx, s);
                }

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
            int rc = waggle::Run(config, callbacks);
            if (state.m_jobSystem)
                state.m_jobSystem->Stop();
            return rc;
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
