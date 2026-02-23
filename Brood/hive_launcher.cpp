#include <waggle/engine_runner.h>
#include <waggle/project/project_manager.h>
#include <waggle/project/project_context.h>
#include <waggle/project/gameplay_module.h>
#include <nectar/project/project_file.h>

#include <hive/core/moduleregistry.h>
#include <hive/core/module.h>
#include <hive/core/log.h>
#include <hive/HiveConfig.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
#include <swarm/swarm.h>
#endif

#if HIVE_MODE_EDITOR
#include <forge/imgui_integration.h>
#include <forge/hierarchy_panel.h>
#include <forge/inspector_panel.h>
#include <forge/asset_browser.h>
#include <forge/toolbar.h>
#include <forge/selection.h>
#include <forge/undo.h>
#include <terra/platform/glfw_terra.h>
#include <queen/reflect/component_registry.h>
#include <imgui.h>
#include <imgui_internal.h>
#endif

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace
{

static const hive::LogCategory LogLauncher{"Hive.Launcher"};

class LauncherModule : public hive::Module
{
public:
    LauncherModule() : logger_{log_mgr_} {}
    const char* GetName() const override { return "LauncherModule"; }
protected:
    void DoInitialize() override { Module::DoInitialize(); }
    void DoShutdown() override { Module::DoShutdown(); }
private:
    hive::LogManager log_mgr_;
    hive::ConsoleLogger logger_;
};

void RegisterLauncherModule()
{
    hive::ModuleRegistry::GetInstance().RegisterModule([]() -> std::unique_ptr<hive::Module> {
        return std::make_unique<LauncherModule>();
    });
}

struct LauncherState
{
    comb::ModuleAllocator alloc{"Launcher", 1024 * 1024 * 1024};
    waggle::ProjectManager* project{nullptr};
    waggle::GameplayModule gameplay;
    const char* project_path{nullptr};

#if HIVE_MODE_EDITOR
    forge::EditorSelection selection;
    std::unique_ptr<forge::UndoStack> undo{std::make_unique<forge::UndoStack>()};
    forge::GizmoState gizmo;
    forge::PlayState play_state{forge::PlayState::Editing};
    queen::ComponentRegistry<256> component_registry;
    std::string assets_root;
    bool first_frame{true};
    swarm::ViewportRT* viewport_rt{nullptr};
    void* viewport_texture{nullptr};
#endif
};

#if HIVE_MODE_EDITOR
void SetupDefaultDockLayout(ImGuiID dockspace_id)
{
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID center = dockspace_id;
    ImGuiID left   = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
    ImGuiID right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);

    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Asset Browser", bottom);
    ImGui::DockBuilderDockWindow("Viewport", center);

    ImGui::DockBuilderFinish(dockspace_id);
}

void DrawEditor(waggle::EngineContext& ctx, LauncherState& state)
{
    // Fullscreen dockspace
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("HiveEditorDockSpace");
    if (state.first_frame)
    {
        SetupDefaultDockLayout(dockspace_id);
        state.first_frame = false;
    }
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Menu bar with toolbar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
                ctx.app->RequestStop();
            ImGui::EndMenu();
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 2.f);

        forge::ToolbarAction action = forge::DrawToolbarButtons(state.play_state, state.gizmo);
        if (action.play_pressed)
            state.play_state = forge::PlayState::Playing;
        if (action.pause_pressed)
            state.play_state = forge::PlayState::Paused;
        if (action.stop_pressed)
            state.play_state = forge::PlayState::Editing;

        ImGui::EndMenuBar();
    }

    ImGui::End(); // DockSpace

    // Hierarchy
    if (ImGui::Begin("Hierarchy"))
        forge::DrawHierarchyPanel(*ctx.world, state.selection);
    ImGui::End();

    // Inspector
    if (ImGui::Begin("Inspector"))
        forge::DrawInspectorPanel(*ctx.world, state.selection, state.component_registry, *state.undo);
    ImGui::End();

    // Asset Browser
    if (ImGui::Begin("Asset Browser"))
        forge::DrawAssetBrowser(state.assets_root.c_str());
    ImGui::End();

    // Viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("Viewport"))
    {
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (state.viewport_rt && state.viewport_texture && size.x > 0 && size.y > 0)
        {
            uint32_t w = static_cast<uint32_t>(size.x);
            uint32_t h = static_cast<uint32_t>(size.y);
            if (w != swarm::GetViewportRTWidth(state.viewport_rt) || h != swarm::GetViewportRTHeight(state.viewport_rt))
            {
                forge::ForgeUnregisterViewportRT(state.viewport_texture);
                swarm::ResizeViewportRT(state.viewport_rt, w, h);
                state.viewport_texture = forge::ForgeRegisterViewportRT(ctx.render_context, state.viewport_rt);
            }
            ImGui::Image(state.viewport_texture, size);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
#endif

} // anonymous namespace

int main(int argc, char* argv[])
{
    std::error_code ec;
    std::filesystem::path project_path;

    if (argc >= 2)
    {
        project_path = argv[1];
    }
    else
    {
        project_path = std::filesystem::current_path(ec) / "project.hive";
    }

    if (!std::filesystem::exists(project_path, ec) || ec)
    {
        std::fprintf(stderr, "Error: project file not found: %s\n",
                     project_path.string().c_str());
        std::fprintf(stderr, "Usage: hive_launcher [path/to/project.hive]\n");
        return 1;
    }

    project_path = std::filesystem::absolute(project_path, ec);
    auto path_str = project_path.generic_string();

    std::string window_title = "HiveEngine";
    {
        comb::ModuleAllocator tmp_alloc{"TmpProjectParse", 4 * 1024 * 1024};
        nectar::ProjectFile pf{tmp_alloc.Get()};
        auto result = pf.LoadFromDisk({path_str.c_str(), path_str.size()});
        if (result.success && pf.Name().Size() > 0)
        {
            window_title += " — ";
            window_title.append(pf.Name().Data(), pf.Name().Size());
        }
    }

    LauncherState state{};
    state.project_path = path_str.c_str();

    waggle::EngineConfig config{};
    config.window_title = window_title.c_str();
#if HIVE_MODE_EDITOR
    config.window_width = 1920;
    config.window_height = 1080;
    config.mode = waggle::EngineMode::Editor;
#elif HIVE_MODE_HEADLESS
    config.mode = waggle::EngineMode::Headless;
#endif

    waggle::EngineCallbacks callbacks{};
    callbacks.on_register_modules = RegisterLauncherModule;

    callbacks.on_setup = [](waggle::EngineContext& ctx, void* ud) -> bool {
        auto& s = *static_cast<LauncherState*>(ud);
        auto& alloc = s.alloc.Get();
        auto& world = *ctx.world;

        s.project = comb::New<waggle::ProjectManager>(alloc, alloc);
        waggle::ProjectConfig proj_config{.enable_hot_reload = true, .watcher_interval_ms = 500};
        if (!s.project->Open({s.project_path, std::strlen(s.project_path)}, proj_config))
        {
            hive::LogError(LogLauncher, "Failed to open project: {}", s.project_path);
            comb::Delete(alloc, s.project);
            s.project = nullptr;
            return false;
        }

        const auto& proj = s.project->Project();
        hive::LogInfo(LogLauncher, "Project '{}' v{}",
                      std::string{proj.Name().Data(), proj.Name().Size()},
                      std::string{proj.Version().Data(), proj.Version().Size()});

        world.InsertResource(waggle::ProjectContext{s.project});

#if HIVE_MODE_EDITOR
        if (ctx.render_context && ctx.window)
        {
            forge::ForgeImGuiInit(ctx.render_context, ctx.window->window_);
            s.viewport_rt = swarm::CreateViewportRT(ctx.render_context, 1280, 720);
            s.viewport_texture = forge::ForgeRegisterViewportRT(ctx.render_context, s.viewport_rt);
        }
#endif

        auto root = std::string{s.project->Paths().root.CStr(), s.project->Paths().root.Size()};
#if HIVE_MODE_EDITOR
        s.assets_root = root + "/assets";
#endif
#if HIVE_PLATFORM_WINDOWS
        auto dll_path = root + "/gameplay.dll";
#else
        auto dll_path = root + "/gameplay.so";
#endif

        std::error_code ec;
        if (std::filesystem::exists(dll_path, ec) && !ec)
        {
            if (s.gameplay.Load(dll_path.c_str()))
            {
                if (!s.gameplay.Register(world))
                    hive::LogWarning(LogLauncher, "Gameplay DLL Register() failed");
            }
            else
                hive::LogWarning(LogLauncher, "Failed to load gameplay DLL: {}", s.gameplay.GetError());
        }
        else
        {
            hive::LogInfo(LogLauncher, "No gameplay DLL found at {}", dll_path);
        }

        return true;
    };

    callbacks.on_frame = [](waggle::EngineContext& ctx, void* ud) {
        auto& s = *static_cast<LauncherState*>(ud);
        s.project->Update();

#if HIVE_MODE_EDITOR
        if (ctx.render_context)
        {
            if (s.viewport_rt)
            {
                swarm::BeginViewportRT(ctx.render_context, s.viewport_rt);
                swarm::DrawPipeline(ctx.render_context);
                swarm::EndViewportRT(ctx.render_context, s.viewport_rt);
            }

            forge::ForgeImGuiNewFrame();
            DrawEditor(ctx, s);
            forge::ForgeImGuiRender(ctx.render_context);
        }
#elif HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
        if (ctx.render_context)
            swarm::DrawPipeline(ctx.render_context);
#endif
    };

    callbacks.on_shutdown = [](waggle::EngineContext& ctx, void* ud) {
        auto& s = *static_cast<LauncherState*>(ud);
        auto& world = *ctx.world;

#if HIVE_MODE_EDITOR
        if (ctx.render_context)
        {
            if (s.viewport_texture)
                forge::ForgeUnregisterViewportRT(s.viewport_texture);
            if (s.viewport_rt)
                swarm::DestroyViewportRT(s.viewport_rt);
            forge::ForgeImGuiShutdown(ctx.render_context);
        }
#endif

        if (s.gameplay.IsRegistered())
            s.gameplay.Unregister(world);

        if (s.project)
        {
            world.RemoveResource<waggle::ProjectContext>();
            s.project->Close();
            comb::Delete(s.alloc.Get(), s.project);
            s.project = nullptr;
        }

        // gameplay.Unload() is NOT called here: the World still has system
        // lambdas whose code lives in the DLL (e.g. FreeCamera). Calling
        // FreeLibrary now would unmap that code while the World still
        // references it. Instead, GameplayModule::~GameplayModule() will
        // call FreeLibrary after Run() returns and the World is destroyed.
    };

    callbacks.user_data = &state;
    return waggle::Run(config, callbacks);
}
