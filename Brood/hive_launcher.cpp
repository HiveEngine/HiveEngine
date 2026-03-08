#include <hive/HiveConfig.h>
#include <hive/core/log.h>

#include <comb/debug/global_memory_tracker.h>
#include <comb/default_allocator.h>
#include <comb/new.h>

#include <nectar/project/project_file.h>

#include <waggle/engine_runner.h>
#include <waggle/project/gameplay_module.h>
#include <waggle/project/project_context.h>
#include <waggle/project/project_manager.h>

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
#include <swarm/swarm.h>
#endif

#if HIVE_MODE_EDITOR
#include <queen/reflect/component_registry.h>

#include <terra/platform/glfw_terra.h>

#include <forge/asset_browser.h>
#include <forge/hierarchy_panel.h>
#include <forge/imgui_integration.h>
#include <forge/inspector_panel.h>
#include <forge/selection.h>
#include <forge/toolbar.h>
#include <forge/undo.h>

#include <imgui.h>

#include <imgui_internal.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace
{

    static const hive::LogCategory LOG_LAUNCHER{"Hive.Launcher"};

    struct LauncherState
    {
        comb::ModuleAllocator m_alloc{"Launcher", size_t{1024} * 1024 * 1024};
        waggle::ProjectManager* m_project{nullptr};
        waggle::GameplayModule m_gameplay;
        const char* m_projectPath{nullptr};
        bool m_exitAfterSetup{false};

#if HIVE_MODE_EDITOR
        forge::EditorSelection m_selection;
        std::unique_ptr<forge::UndoStack> m_undo{std::make_unique<forge::UndoStack>()};
        forge::GizmoState m_gizmo;
        forge::PlayState m_playState{forge::PlayState::EDITING};
        queen::ComponentRegistry<256> m_componentRegistry;
        std::string m_assetsRoot;
        bool m_firstFrame{true};
        swarm::ViewportRT* m_viewportRt{nullptr};
        void* m_viewportTexture{nullptr};
#endif
    };

#if HIVE_MODE_EDITOR
    void DrawEditor(waggle::EngineContext& ctx, LauncherState& state);
#endif

    struct LauncherCommandLine
    {
        std::filesystem::path m_projectPath{};
        bool m_forceHeadless{false};
        bool m_exitAfterSetup{false};
    };

    void PrintUsage() {
        std::fprintf(stderr, "Usage: hive_launcher [--headless] [--exit-after-setup] [path/to/project.hive]\n");
    }

    bool TryParseCommandLine(int argc, char* argv[], LauncherCommandLine* commandLine) {
        for (int i = 1; i < argc; ++i)
        {
            const char* arg = argv[i];
            if (std::strcmp(arg, "--headless") == 0)
            {
                commandLine->m_forceHeadless = true;
                continue;
            }

            if (std::strcmp(arg, "--exit-after-setup") == 0)
            {
                commandLine->m_exitAfterSetup = true;
                continue;
            }

            if (arg[0] == '-')
            {
                std::fprintf(stderr, "Error: unknown option: %s\n", arg);
                PrintUsage();
                return false;
            }

            if (!commandLine->m_projectPath.empty())
            {
                std::fprintf(stderr, "Error: multiple project paths provided\n");
                PrintUsage();
                return false;
            }

            commandLine->m_projectPath = arg;
        }

        return true;
    }

    std::string BuildWindowTitle(const std::string& projectPath) {
        std::string windowTitle = "HiveEngine";
        comb::ModuleAllocator tmpAlloc{"TmpProjectParse", size_t{4} * 1024 * 1024};
        nectar::ProjectFile projectFile{tmpAlloc.Get()};
        auto loadResult = projectFile.LoadFromDisk({projectPath.c_str(), projectPath.size()});
        if (loadResult.m_success && projectFile.Name().Size() > 0)
        {
            windowTitle += " - ";
            windowTitle.append(projectFile.Name().Data(), projectFile.Name().Size());
        }

        return windowTitle;
    }

    class LauncherProcessSession
    {
    public:
        LauncherProcessSession()
            : m_logger{m_logManager} {}

        ~LauncherProcessSession() { Finalize(); }

        int Run(int argc, char* argv[]) {
            LauncherCommandLine commandLine{};
            if (!TryParseCommandLine(argc, argv, &commandLine))
            {
                return 1;
            }

            return Run(commandLine);
        }

        void Finalize() {
            if (m_finalized)
            {
                return;
            }

            comb::debug::ReportLiveAllocatorLeaks();
            hive::ShutdownProfiler();
            std::cout.flush();
            std::cerr.flush();
            std::fflush(stdout);
            std::fflush(stderr);
            m_finalized = true;
        }

    private:
        int Run(const LauncherCommandLine& commandLine) {
            std::error_code ec;
            std::filesystem::path projectPath = commandLine.m_projectPath;
            if (projectPath.empty())
            {
                projectPath = std::filesystem::current_path(ec) / "project.hive";
            }

            if (!std::filesystem::exists(projectPath, ec) || ec)
            {
                std::fprintf(stderr, "Error: project file not found: %s\n", projectPath.string().c_str());
                PrintUsage();
                return 1;
            }

            projectPath = std::filesystem::absolute(projectPath, ec);
            if (ec)
            {
                std::fprintf(stderr, "Error: failed to resolve project path: %s\n", projectPath.string().c_str());
                return 1;
            }

            const std::string pathStr = projectPath.generic_string();
            const std::string windowTitle = BuildWindowTitle(pathStr);

            LauncherState state{};
            state.m_projectPath = pathStr.c_str();
            state.m_exitAfterSetup = commandLine.m_exitAfterSetup;

            waggle::EngineConfig config{};
            config.m_windowTitle = windowTitle.c_str();
#if HIVE_MODE_EDITOR
            config.m_windowWidth = 1920;
            config.m_windowHeight = 1080;
            config.m_mode = commandLine.m_forceHeadless ? waggle::EngineMode::HEADLESS : waggle::EngineMode::EDITOR;
#elif HIVE_MODE_HEADLESS
            config.m_mode = waggle::EngineMode::HEADLESS;
#else
            config.m_mode = commandLine.m_forceHeadless ? waggle::EngineMode::HEADLESS : waggle::EngineMode::GAME;
#endif

            waggle::EngineCallbacks callbacks{};
            callbacks.m_onSetup = [](waggle::EngineContext& ctx, void* ud) -> bool {
                auto& s = *static_cast<LauncherState*>(ud);
                auto& alloc = s.m_alloc.Get();
                auto& world = *ctx.m_world;

                s.m_project = comb::New<waggle::ProjectManager>(alloc, alloc);
                const waggle::ProjectConfig projConfig{.m_enableHotReload = true, .m_watcherIntervalMs = 500};
                if (!s.m_project->Open({s.m_projectPath, std::strlen(s.m_projectPath)}, projConfig))
                {
                    hive::LogError(LOG_LAUNCHER, "Failed to open project: {}", s.m_projectPath);
                    comb::Delete(alloc, s.m_project);
                    s.m_project = nullptr;
                    return false;
                }

                const auto& proj = s.m_project->Project();
                hive::LogInfo(LOG_LAUNCHER, "Project '{}' v{}", std::string{proj.Name().Data(), proj.Name().Size()},
                              std::string{proj.Version().Data(), proj.Version().Size()});

                world.InsertResource(waggle::ProjectContext{s.m_project});

#if HIVE_MODE_EDITOR
                if (ctx.m_renderContext && ctx.m_window)
                {
                    forge::ForgeImGuiInit(ctx.m_renderContext, ctx.m_window->m_window);
                    s.m_viewportRt = swarm::CreateViewportRT(ctx.m_renderContext, 1280, 720);
                    s.m_viewportTexture = forge::ForgeRegisterViewportRT(ctx.m_renderContext, s.m_viewportRt);
                }
#endif

                const auto root = std::string{s.m_project->Paths().m_root.CStr(), s.m_project->Paths().m_root.Size()};
#if HIVE_MODE_EDITOR
                s.m_assetsRoot = root + "/assets";
#endif
#if HIVE_PLATFORM_WINDOWS
                const auto dllPath = root + "/gameplay.dll";
#else
                const auto dllPath = root + "/gameplay.so";
#endif

                std::error_code dllEc;
                if (std::filesystem::exists(dllPath, dllEc) && !dllEc)
                {
                    if (s.m_gameplay.Load(dllPath.c_str()))
                    {
                        if (!s.m_gameplay.Register(world))
                        {
                            hive::LogWarning(LOG_LAUNCHER, "Gameplay DLL Register() failed");
                        }
                    }
                    else
                    {
                        hive::LogWarning(LOG_LAUNCHER, "Failed to load gameplay DLL: {}", s.m_gameplay.GetError());
                    }
                }
                else
                {
                    hive::LogInfo(LOG_LAUNCHER, "No gameplay DLL found at {}", dllPath);
                }

                if (s.m_exitAfterSetup)
                {
                    ctx.m_app->RequestStop();
                }

                return true;
            };

            callbacks.m_onFrame = [](waggle::EngineContext& ctx, void* ud) {
                auto& s = *static_cast<LauncherState*>(ud);
                s.m_project->Update();

#if HIVE_MODE_EDITOR
                if (ctx.m_renderContext)
                {
                    if (s.m_viewportRt)
                    {
                        swarm::BeginViewportRT(ctx.m_renderContext, s.m_viewportRt);
                        swarm::DrawPipeline(ctx.m_renderContext);
                        swarm::EndViewportRT(ctx.m_renderContext, s.m_viewportRt);
                    }

                    forge::ForgeImGuiNewFrame();
                    DrawEditor(ctx, s);
                    forge::ForgeImGuiRender(ctx.m_renderContext);
                }
#elif HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                if (ctx.m_renderContext)
                {
                    swarm::DrawPipeline(ctx.m_renderContext);
                }
#endif
            };

            callbacks.m_onShutdown = [](waggle::EngineContext& ctx, void* ud) {
                auto& s = *static_cast<LauncherState*>(ud);
                auto& world = *ctx.m_world;

#if HIVE_MODE_EDITOR
                if (ctx.m_renderContext)
                {
                    swarm::WaitForIdle(ctx.m_renderContext);
                    if (s.m_viewportTexture)
                    {
                        forge::ForgeUnregisterViewportRT(s.m_viewportTexture);
                    }
                    if (s.m_viewportRt)
                    {
                        swarm::DestroyViewportRT(s.m_viewportRt);
                    }
                    forge::ForgeImGuiShutdown(ctx.m_renderContext);
                }
#endif

                if (s.m_gameplay.IsRegistered())
                {
                    s.m_gameplay.Unregister(world);
                }

                if (s.m_project)
                {
                    world.RemoveResource<waggle::ProjectContext>();
                    s.m_project->Close();
                    comb::Delete(s.m_alloc.Get(), s.m_project);
                    s.m_project = nullptr;
                }

                // gameplay.Unload() is NOT called here: the World still has system
                // lambdas whose code lives in the DLL (e.g. FreeCamera). Calling
                // FreeLibrary now would unmap that code while the World still
                // references it. Instead, GameplayModule::~GameplayModule() will
                // call FreeLibrary after Run() returns and the World is destroyed.
            };

            callbacks.m_userData = &state;
            return waggle::Run(config, callbacks);
        }

        hive::LogManager m_logManager;
        hive::ConsoleLogger m_logger;
        bool m_finalized{false};
    };

#if HIVE_MODE_EDITOR
    void SetupDefaultDockLayout(ImGuiID dockspaceId) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

        ImGuiID center = dockspaceId;
        const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
        const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
        const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);

        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Asset Browser", bottom);
        ImGui::DockBuilderDockWindow("Viewport", center);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void DrawEditor(waggle::EngineContext& ctx, LauncherState& state) {
        // Fullscreen dockspace
        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
                                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceId = ImGui::GetID("HiveEditorDockSpace");
        if (state.m_firstFrame)
        {
            SetupDefaultDockLayout(dockspaceId);
            state.m_firstFrame = false;
        }
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // Menu bar with toolbar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit"))
                    ctx.m_app->RequestStop();
                ImGui::EndMenu();
            }

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 2.f);

            const forge::ToolbarAction action = forge::DrawToolbarButtons(state.m_playState, state.m_gizmo);
            if (action.m_playPressed)
                state.m_playState = forge::PlayState::PLAYING;
            if (action.m_pausePressed)
                state.m_playState = forge::PlayState::PAUSED;
            if (action.m_stopPressed)
                state.m_playState = forge::PlayState::EDITING;

            ImGui::EndMenuBar();
        }

        ImGui::End(); // DockSpace

        // Hierarchy
        if (ImGui::Begin("Hierarchy"))
            forge::DrawHierarchyPanel(*ctx.m_world, state.m_selection);
        ImGui::End();

        // Inspector
        if (ImGui::Begin("Inspector"))
            forge::DrawInspectorPanel(*ctx.m_world, state.m_selection, state.m_componentRegistry, *state.m_undo);
        ImGui::End();

        // Asset Browser
        if (ImGui::Begin("Asset Browser"))
            forge::DrawAssetBrowser(state.m_assetsRoot.c_str());
        ImGui::End();

        // Viewport
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::Begin("Viewport"))
        {
            const ImVec2 size = ImGui::GetContentRegionAvail();
            if (state.m_viewportRt != nullptr && state.m_viewportTexture != nullptr && size.x > 0 && size.y > 0)
            {
                const uint32_t w = static_cast<uint32_t>(size.x);
                const uint32_t h = static_cast<uint32_t>(size.y);
                if (w != swarm::GetViewportRTWidth(state.m_viewportRt) ||
                    h != swarm::GetViewportRTHeight(state.m_viewportRt))
                {
                    forge::ForgeUnregisterViewportRT(state.m_viewportTexture);
                    swarm::ResizeViewportRT(state.m_viewportRt, w, h);
                    state.m_viewportTexture = forge::ForgeRegisterViewportRT(ctx.m_renderContext, state.m_viewportRt);
                }
                ImGui::Image(state.m_viewportTexture, size);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
#endif

} // anonymous namespace

int main(int argc, char* argv[]) {
    LauncherProcessSession session{};
    const int result = session.Run(argc, argv);
    session.Finalize();
    // Runtime teardown is complete here. Avoid lingering during CRT static teardown.
    std::_Exit(result);
}
