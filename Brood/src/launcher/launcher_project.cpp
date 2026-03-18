#include <launcher/launcher_project.h>

#include <hive/HiveConfig.h>
#include <hive/core/log.h>

#include <waggle/app_context.h>
#include <waggle/project/project_context.h>
#include <waggle/project/project_scaffolder.h>

#include <terra/terra.h>

#include <string>

#include <launcher/launcher_platform.h>
#include <launcher/launcher_scene.h>

namespace brood::launcher
{

    void TryLoadGameplayModule(waggle::EngineContext& ctx, LauncherState& state)
    {
        const auto root = std::string{state.m_project->Paths().m_root.CStr(), state.m_project->Paths().m_root.Size()};

#if HIVE_MODE_EDITOR
        state.m_assetsRoot =
            std::string{state.m_project->Paths().m_assets.CStr(), state.m_project->Paths().m_assets.Size()};
#endif

#if HIVE_PLATFORM_WINDOWS
        const std::string dllPath = root + "/gameplay.dll";
#else
        const std::string dllPath = root + "/gameplay.so";
#endif

        std::error_code dllEc;
        if (std::filesystem::exists(dllPath, dllEc) && !dllEc)
        {
            if (state.m_gameplay.Load(dllPath.c_str()))
            {
                if (!state.m_gameplay.Register(*ctx.m_world))
                {
                    hive::LogWarning(LOG_LAUNCHER, "Gameplay DLL Register() failed");
                }
            }
            else
            {
                hive::LogWarning(LOG_LAUNCHER, "Failed to load gameplay DLL: {}", state.m_gameplay.GetError());
            }
        }
        else
        {
            hive::LogInfo(LOG_LAUNCHER, "No gameplay DLL found at {}", dllPath);
        }
    }

    bool OpenProject(waggle::EngineContext& ctx, LauncherState& state, const std::filesystem::path& requestedPath)
    {
        if (state.m_project == nullptr)
        {
            return false;
        }

        if (state.m_projectOpen)
        {
#if HIVE_MODE_EDITOR
            SetHubStatus(state.m_hub, true, "A project is already open in this launcher session.");
#endif
            return false;
        }

        std::filesystem::path resolvedPath{};
        if (!ResolveProjectFilePath(requestedPath, &resolvedPath))
        {
#if HIVE_MODE_EDITOR
            SetHubStatus(state.m_hub, true, "The selected path does not contain a valid project.hive file.");
#endif
            return false;
        }

        const std::string projectPath = resolvedPath.generic_string();
        const waggle::ProjectConfig projectConfig{
            .m_enableHotReload = HIVE_FEATURE_HOT_RELOAD != 0,
            .m_watcherIntervalMs = 500,
        };

        if (!state.m_project->Open({projectPath.c_str(), projectPath.size()}, projectConfig))
        {
#if HIVE_MODE_EDITOR
            SetHubStatus(state.m_hub, true, "Failed to open the selected project.");
#endif
            hive::LogError(LOG_LAUNCHER, "Failed to open project: {}", projectPath);
            return false;
        }

        state.m_projectPath = projectPath;
        state.m_projectOpen = true;

        const auto& project = state.m_project->Project();
        hive::LogInfo(LOG_LAUNCHER, "Project '{}' v{}", std::string{project.Name().Data(), project.Name().Size()},
                      std::string{project.Version().Data(), project.Version().Size()});

        ctx.m_world->InsertResource(waggle::ProjectContext{state.m_project});

        if (ctx.m_window != nullptr)
        {
            const std::string windowTitle = BuildWindowTitle(projectPath);
            terra::SetWindowTitle(ctx.m_window, windowTitle.c_str());
        }

#if HIVE_MODE_EDITOR
        ResetSceneEditorState(state);
        state.m_currentScenePath.clear();
        state.m_currentSceneRelative.clear();

        const wax::StringView startupScene = state.m_project->Project().StartupSceneRelative();
        if (!startupScene.IsEmpty())
        {
            const std::filesystem::path startupScenePath = ResolveScenePath(*state.m_project, startupScene);
            if (!LoadEditorScene(ctx, state, startupScenePath))
            {
                hive::LogWarning(LOG_LAUNCHER, "Failed to load startup scene: {}", startupScenePath.generic_string());
            }
        }
        QueueSceneRecoveryPrompt(state);
#endif
        TryLoadGameplayModule(ctx, state);

#if HIVE_MODE_EDITOR
        SetHubStatus(state.m_hub, false, "Project ready.");
#endif

        if (state.m_exitAfterSetup)
        {
            ctx.m_app->RequestStop();
        }

        return true;
    }

#if HIVE_MODE_EDITOR
    bool CreateProjectFromHub(waggle::EngineContext& ctx, LauncherState& state)
    {
        ProjectHubState& hub = state.m_hub;
        const std::string projectName = TrimmedCopy(hub.m_createName.data());
        if (!IsProjectNameValid(projectName))
        {
            SetHubStatus(hub, true, "Project name must use only letters, numbers, '_' or '-'.");
            return false;
        }

        const std::string version = TrimmedCopy(hub.m_createVersion.data()).empty()
                                        ? std::string{"0.1.0"}
                                        : TrimmedCopy(hub.m_createVersion.data());
        if (hub.m_engineRoot.empty())
        {
            SetHubStatus(hub, true,
                         "Unable to locate the HiveEngine root. Set HIVE_ENGINE_DIR or run from an engine build.");
            return false;
        }

        if (!hub.m_supportEditor && !hub.m_supportGame && !hub.m_supportHeadless)
        {
            SetHubStatus(hub, true, "Enable at least one runtime mode before creating the project.");
            return false;
        }

        const std::filesystem::path targetRoot = BuildCreateTargetPath(hub, projectName);
        if (targetRoot.empty())
        {
            SetHubStatus(hub, true, "Choose a destination directory before creating the project.");
            return false;
        }

        std::error_code ec;
        if (std::filesystem::exists(targetRoot, ec) && !ec)
        {
            if (!std::filesystem::is_directory(targetRoot, ec))
            {
                SetHubStatus(hub, true, "The destination already exists and is not a directory.");
                return false;
            }

            std::filesystem::directory_iterator end{};
            std::filesystem::directory_iterator it{targetRoot, ec};
            if (!ec && it != end)
            {
                SetHubStatus(hub, true, "The destination directory is not empty.");
                return false;
            }
        }

        const bool wantsGraphics = hub.m_supportEditor || hub.m_supportGame;
        const std::string engineRoot = hub.m_engineRoot.generic_string();
        const std::string targetRootString = targetRoot.generic_string();
        const std::string presetBase = GetPresetBase(hub.m_toolchain);
        const std::string runtimeBackend = wantsGraphics ? "vulkan" : "";

        waggle::ProjectScaffoldConfig scaffoldConfig{};
        scaffoldConfig.m_cmake.m_projectName = {projectName.c_str(), projectName.size()};
        scaffoldConfig.m_cmake.m_projectRoot = {targetRootString.c_str(), targetRootString.size()};
        scaffoldConfig.m_cmake.m_enginePath = {engineRoot.c_str(), engineRoot.size()};
        scaffoldConfig.m_cmake.m_linkSwarm = wantsGraphics;
        scaffoldConfig.m_cmake.m_linkTerra = wantsGraphics;
        scaffoldConfig.m_cmake.m_linkAntennae = wantsGraphics;
        scaffoldConfig.m_projectVersion = {version.c_str(), version.size()};
        scaffoldConfig.m_runtimeBackend = {runtimeBackend.c_str(), runtimeBackend.size()};
        scaffoldConfig.m_presetBase = {presetBase.c_str(), presetBase.size()};
        scaffoldConfig.m_supportEditor = hub.m_supportEditor;
        scaffoldConfig.m_supportGame = hub.m_supportGame;
        scaffoldConfig.m_supportHeadless = hub.m_supportHeadless;

        if (!waggle::ProjectScaffolder::WriteToProject(scaffoldConfig, state.m_alloc.Get()))
        {
            SetHubStatus(hub, true, "Project files could not be generated.");
            return false;
        }

        RefreshProjectHub(hub);
        CopyStringToBuffer((targetRoot / "project.hive").generic_string(), hub.m_openPath.data(),
                           hub.m_openPath.size());

        return OpenProject(ctx, state, targetRoot / "project.hive");
    }
#endif

} // namespace brood::launcher
