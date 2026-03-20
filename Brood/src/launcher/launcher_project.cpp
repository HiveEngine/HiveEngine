#include <hive/hive_config.h>
#include <hive/core/log.h>

#include <wax/containers/string.h>

#include <nectar/mesh/gltf_importer.h>
#include <nectar/texture/texture_importer.h>

#include <waggle/app_context.h>
#include <waggle/project/project_context.h>
#include <waggle/project/project_scaffolder.h>

#include <terra/terra.h>

#include <launcher/launcher_platform.h>
#include <launcher/launcher_project.h>
#include <launcher/launcher_scene.h>

namespace brood::launcher
{

    static wax::String ResolveGameplayDllPath(const wax::String& projectRoot)
    {
#if HIVE_PLATFORM_WINDOWS
        constexpr const char* kDllName = "gameplay.dll";
#else
        constexpr const char* kDllName = "gameplay.so";
#endif

#if HIVE_CONFIG_DEBUG
        constexpr const char* kConfig = "Debug";
#elif HIVE_CONFIG_RELEASE
        constexpr const char* kConfig = "Release";
#elif HIVE_CONFIG_PROFILE
        constexpr const char* kConfig = "Profile";
#elif HIVE_CONFIG_RETAIL
        constexpr const char* kConfig = "Retail";
#else
        constexpr const char* kConfig = "Debug";
#endif

        std::error_code ec;

        auto configPath = projectRoot + "/.hive/modules/" + kConfig + "/" + kDllName;
        if (std::filesystem::exists(std::filesystem::path{configPath.CStr()}, ec) && !ec)
            return configPath;

        auto basePath = projectRoot + "/.hive/modules/" + kDllName;
        if (std::filesystem::exists(std::filesystem::path{basePath.CStr()}, ec) && !ec)
            return basePath;

        auto rootPath = projectRoot + "/" + kDllName;
        if (std::filesystem::exists(std::filesystem::path{rootPath.CStr()}, ec) && !ec)
            return rootPath;

        return {};
    }

    void TryLoadGameplayModule(waggle::EngineContext& ctx, LauncherState& state)
    {
        const wax::String root{state.m_project->Paths().m_root};

#if HIVE_MODE_EDITOR
        state.m_assetsRoot = state.m_project->Paths().m_assets;
#endif

        const wax::String dllPath = ResolveGameplayDllPath(root);
        if (dllPath.IsEmpty())
        {
            hive::LogInfo(LOG_LAUNCHER, "No gameplay DLL found for project at {}", root.CStr());
            return;
        }

        if (state.m_gameplay.Load(dllPath.CStr()))
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

        const wax::String projectPath{resolvedPath.generic_string().c_str()};
        const waggle::ProjectConfig projectConfig{
            .m_enableHotReload = HIVE_FEATURE_HOT_RELOAD != 0,
            .m_watcherIntervalMs = 500,
        };

        if (!state.m_project->Open({projectPath.CStr(), projectPath.Size()}, projectConfig))
        {
#if HIVE_MODE_EDITOR
            SetHubStatus(state.m_hub, true, "Failed to open the selected project.");
#endif
            hive::LogError(LOG_LAUNCHER, "Failed to open project: {}", projectPath.CStr());
            return false;
        }

        state.m_projectPath = projectPath;
        state.m_projectOpen = true;

        // Stateless singletons — no init order dependency, no mutable state
        static nectar::GltfImporter s_gltfImporter;
        static nectar::TextureImporter s_textureImporter;
        state.m_project->RegisterImporter(&s_gltfImporter);
        state.m_project->RegisterImporter(&s_textureImporter);

        const auto& project = state.m_project->Project();
        hive::LogInfo(LOG_LAUNCHER, "Project '{}' v{}", std::string_view{project.Name().Data(), project.Name().Size()},
                      std::string_view{project.Version().Data(), project.Version().Size()});

        ctx.m_world->InsertResource(waggle::ProjectContext{state.m_project});

        if (ctx.m_window != nullptr)
        {
            const wax::String windowTitle = BuildWindowTitle(projectPath);
            terra::SetWindowTitle(ctx.m_window, windowTitle.CStr());
        }

#if HIVE_MODE_EDITOR
        ResetSceneEditorState(state);
        state.m_currentScenePath.Clear();
        state.m_currentSceneRelative.Clear();

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
        const wax::String projectName = TrimmedCopy(hub.m_createName.Data());
        if (!IsProjectNameValid(projectName))
        {
            SetHubStatus(hub, true, "Project name must use only letters, numbers, '_' or '-'.");
            return false;
        }

        const wax::String version = TrimmedCopy(hub.m_createVersion.Data()).IsEmpty()
                                        ? wax::String{"0.1.0"}
                                        : TrimmedCopy(hub.m_createVersion.Data());
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
        const wax::String engineRoot{hub.m_engineRoot.generic_string().c_str()};
        const wax::String targetRootString{targetRoot.generic_string().c_str()};
        const wax::String presetBase{GetPresetBase(hub.m_toolchain)};
        const wax::String runtimeBackend{wantsGraphics ? "vulkan" : ""};

        waggle::ProjectScaffoldConfig scaffoldConfig{};
        scaffoldConfig.m_cmake.m_projectName = {projectName.CStr(), projectName.Size()};
        scaffoldConfig.m_cmake.m_projectRoot = {targetRootString.CStr(), targetRootString.Size()};
        scaffoldConfig.m_cmake.m_enginePath = {engineRoot.CStr(), engineRoot.Size()};
        scaffoldConfig.m_cmake.m_linkSwarm = wantsGraphics;
        scaffoldConfig.m_cmake.m_linkTerra = wantsGraphics;
        scaffoldConfig.m_cmake.m_linkAntennae = wantsGraphics;
        scaffoldConfig.m_projectVersion = {version.CStr(), version.Size()};
        scaffoldConfig.m_runtimeBackend = {runtimeBackend.CStr(), runtimeBackend.Size()};
        scaffoldConfig.m_presetBase = {presetBase.CStr(), presetBase.Size()};
        scaffoldConfig.m_supportEditor = hub.m_supportEditor;
        scaffoldConfig.m_supportGame = hub.m_supportGame;
        scaffoldConfig.m_supportHeadless = hub.m_supportHeadless;

        if (!waggle::ProjectScaffolder::WriteToProject(scaffoldConfig, state.m_alloc.Get()))
        {
            SetHubStatus(hub, true, "Project files could not be generated.");
            return false;
        }

        RefreshProjectHub(hub);
        CopyStringToBuffer(wax::String{(targetRoot / "project.hive").generic_string().c_str()}, hub.m_openPath.Data(),
                           hub.m_openPath.Size());

        return OpenProject(ctx, state, targetRoot / "project.hive");
    }
#endif

} // namespace brood::launcher
