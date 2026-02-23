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

#include <cstdio>
#include <cstring>
#include <filesystem>
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
};

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

        auto root = std::string{s.project->Paths().root.CStr(), s.project->Paths().root.Size()};
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

    callbacks.on_frame = [](waggle::EngineContext&, void* ud) {
        auto& s = *static_cast<LauncherState*>(ud);
        s.project->Update();
    };

    callbacks.on_shutdown = [](waggle::EngineContext& ctx, void* ud) {
        auto& s = *static_cast<LauncherState*>(ud);
        auto& world = *ctx.world;

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
