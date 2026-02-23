#include <waggle/engine_runner.h>

#include <hive/core/moduleregistry.h>
#include <hive/core/log.h>
#include <hive/profiling/profiler.h>

#if HIVE_FEATURE_GLFW
#include <terra/terra.h>
#include <terra/platform/glfw_terra.h>
#include <antennae/input.h>
#include <antennae/keyboard.h>
#endif

static const hive::LogCategory LogEngine{"Waggle.EngineRunner"};

namespace waggle
{

int Run(const EngineConfig& config, const EngineCallbacks& callbacks)
{
    // ---- Module system ----
    hive::ModuleRegistry moduleRegistry;
    if (callbacks.on_register_modules)
        callbacks.on_register_modules();
    moduleRegistry.CreateModules();
    moduleRegistry.ConfigureModules();
    moduleRegistry.InitModules();

    // ---- App (ECS world + fixed timestep) ----
    App app{config.app};
    auto& world = app.GetWorld();

    EngineContext ctx{};
    ctx.app = &app;
    ctx.world = &world;

    bool graphical = (config.mode != EngineMode::Headless);

#if HIVE_FEATURE_GLFW
    terra::WindowContext window_ctx{};
    terra::WindowContext* window_ptr = nullptr;

    if (graphical)
    {
        // ---- Window ----
        if (!terra::InitSystem())
        {
            hive::LogError(LogEngine, "Failed to initialize windowing backend");
            moduleRegistry.ShutdownModules();
            return 1;
        }

        if (!terra::InitWindowContext(&window_ctx))
        {
            hive::LogError(LogEngine, "Failed to create window");
            terra::ShutdownSystem();
            moduleRegistry.ShutdownModules();
            return 1;
        }
        window_ptr = &window_ctx;
        ctx.window = window_ptr;

        // TODO: Initialize Diligent renderer here once the Swarm API is migrated
    }
#else
    (void)graphical;
#endif

    // ---- Setup callback ----
    if (callbacks.on_setup)
    {
        if (!callbacks.on_setup(ctx, callbacks.user_data))
        {
#if HIVE_FEATURE_GLFW
            if (graphical)
            {
                terra::ShutdownWindowContext(window_ptr);
                terra::ShutdownSystem();
            }
#endif
            moduleRegistry.ShutdownModules();
            return 1;
        }
    }

    // ---- Main loop ----
#if HIVE_FEATURE_GLFW
    if (graphical)
    {
        while (!terra::ShouldWindowClose(window_ptr) && app.IsRunning())
        {
            HIVE_PROFILE_SCOPE_N("Frame");
            terra::PollEvents();
            antennae::UpdateInput(world, window_ptr);

            if (config.auto_tick)
                app.Tick();

            if (callbacks.on_frame)
                callbacks.on_frame(ctx, callbacks.user_data);

            // TODO: Render frame with Diligent once migrated
        }
    }
    else
#endif
    {
        while (app.IsRunning())
        {
            HIVE_PROFILE_SCOPE_N("Frame");
            if (config.auto_tick)
                app.Tick();

            if (callbacks.on_frame)
                callbacks.on_frame(ctx, callbacks.user_data);
        }
    }

    // ---- Shutdown callback ----
    if (callbacks.on_shutdown)
        callbacks.on_shutdown(ctx, callbacks.user_data);

    // ---- Cleanup ----
#if HIVE_FEATURE_GLFW
    if (graphical)
    {
        terra::ShutdownWindowContext(window_ptr);
        terra::ShutdownSystem();
    }
#endif

    moduleRegistry.ShutdownModules();
    return 0;
}

} // namespace waggle
