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

#if (HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12) && HIVE_FEATURE_GLFW
#include <swarm/swarm.h>
#include <swarm/platform/diligent_swarm.h>
#ifdef _WIN32
    #define TERRA_NATIVE_WIN32
    #include <terra/terra_native.h>
    #include <swarm/platform/win32_swarm.h>
#elif defined(__linux__)
    #define TERRA_NATIVE_LINUX
    #include <terra/terra_native.h>
    #include <swarm/platform/linux_swarm.h>
#endif
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
    window_ctx.title_ = config.window_title;
    window_ctx.width_ = static_cast<int>(config.window_width);
    window_ctx.height_ = static_cast<int>(config.window_height);
    terra::WindowContext* window_ptr = nullptr;

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
    swarm::RenderContext render_ctx{};
    bool renderer_active = false;
#endif

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

        // ---- Renderer ----
#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
        if (config.auto_renderer)
        {
            if (!swarm::InitSystem())
            {
                hive::LogError(LogEngine, "Failed to initialize Swarm");
                terra::ShutdownWindowContext(window_ptr);
                terra::ShutdownSystem();
                moduleRegistry.ShutdownModules();
                return 1;
            }

            terra::NativeWindow native = terra::GetNativeWindow(window_ptr);
            bool render_ok = false;

#ifdef _WIN32
            render_ok = swarm::InitRenderContextWin32(&render_ctx, native.instance_, native.window_,
                static_cast<uint32_t>(window_ctx.width_), static_cast<uint32_t>(window_ctx.height_));
#elif defined(__linux__)
            switch (native.type_)
            {
                case terra::NativeWindowType::X11:
                    render_ok = swarm::InitRenderContextX11(render_ctx, native.x11Display_, native.x11Window_,
                        static_cast<uint32_t>(window_ctx.width_), static_cast<uint32_t>(window_ctx.height_));
                    break;
                case terra::NativeWindowType::WAYLAND:
                    render_ok = swarm::InitRenderContextWayland(render_ctx, native.wlDisplay_, native.wlSurface_,
                        static_cast<uint32_t>(window_ctx.width_), static_cast<uint32_t>(window_ctx.height_));
                    break;
            }
#endif

            if (!render_ok)
            {
                hive::LogError(LogEngine, "Failed to create render context");
                swarm::ShutdownSystem();
                terra::ShutdownWindowContext(window_ptr);
                terra::ShutdownSystem();
                moduleRegistry.ShutdownModules();
                return 1;
            }

            swarm::SetupGraphicPipeline(render_ctx);
            ctx.render_context = &render_ctx;
            renderer_active = true;
        }
#endif
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
#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                if (renderer_active)
                {
                    swarm::ShutdownRenderContext(render_ctx);
                    swarm::ShutdownSystem();
                }
#endif
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

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
            if (renderer_active)
                swarm::BeginFrame(&render_ctx);
#endif

            if (callbacks.on_frame)
                callbacks.on_frame(ctx, callbacks.user_data);

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
            if (renderer_active)
                swarm::EndFrame(&render_ctx);
#endif
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
#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
        if (renderer_active)
        {
            swarm::ShutdownRenderContext(render_ctx);
            swarm::ShutdownSystem();
        }
#endif
        terra::ShutdownWindowContext(window_ptr);
        terra::ShutdownSystem();
    }
#endif

    moduleRegistry.ShutdownModules();
    return 0;
}

} // namespace waggle
