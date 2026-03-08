#include <hive/core/log.h>
#include <hive/core/moduleregistry.h>
#include <hive/profiling/profiler.h>

#include <waggle/engine_runner.h>

#if HIVE_FEATURE_GLFW
#include <terra/platform/glfw_terra.h>
#include <terra/terra.h>

#include <antennae/input.h>
#include <antennae/keyboard.h>
#endif

#if (HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12) && HIVE_FEATURE_GLFW
#include <swarm/platform/diligent_swarm.h>
#include <swarm/swarm.h>
#ifdef _WIN32
#define TERRA_NATIVE_WIN32
#include <swarm/platform/win32_swarm.h>

#include <terra/terra_native.h>
#elif defined(__linux__)
#define TERRA_NATIVE_LINUX
#include <swarm/platform/linux_swarm.h>

#include <terra/terra_native.h>
#endif
#endif

static const hive::LogCategory LOG_ENGINE{"Waggle.EngineRunner"};

namespace waggle
{

    int Run(const EngineConfig& config, const EngineCallbacks& callbacks) {
        // ---- Module system ----
        hive::ModuleRegistry moduleRegistry;
        if (callbacks.m_onRegisterModules != nullptr)
            callbacks.m_onRegisterModules();
        moduleRegistry.CreateModules();
        moduleRegistry.ConfigureModules();
        moduleRegistry.InitModules();

        // ---- App (ECS world + fixed timestep) ----
        App app{config.m_app};
        auto& world = app.GetWorld();

        EngineContext ctx{};
        ctx.m_app = &app;
        ctx.m_world = &world;

        const bool graphical = (config.m_mode != EngineMode::HEADLESS);

#if HIVE_FEATURE_GLFW
        terra::WindowContext windowCtx{};
        windowCtx.m_title = config.m_windowTitle;
        windowCtx.m_width = static_cast<int>(config.m_windowWidth);
        windowCtx.m_height = static_cast<int>(config.m_windowHeight);
        terra::WindowContext* windowPtr = nullptr;

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
        swarm::RenderContext renderCtx{};
        bool rendererActive = false;
#endif

        if (graphical)
        {
            // ---- Window ----
            if (!terra::InitSystem())
            {
                hive::LogError(LOG_ENGINE, "Failed to initialize windowing backend");
                moduleRegistry.ShutdownModules();
                return 1;
            }

            if (!terra::InitWindowContext(&windowCtx))
            {
                hive::LogError(LOG_ENGINE, "Failed to create window");
                terra::ShutdownSystem();
                moduleRegistry.ShutdownModules();
                return 1;
            }
            windowPtr = &windowCtx;
            ctx.m_window = windowPtr;

            // ---- Renderer ----
#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
            if (config.m_autoRenderer)
            {
                if (!swarm::InitSystem())
                {
                    hive::LogError(LOG_ENGINE, "Failed to initialize Swarm");
                    terra::ShutdownWindowContext(windowPtr);
                    terra::ShutdownSystem();
                    moduleRegistry.ShutdownModules();
                    return 1;
                }

                terra::NativeWindow native = terra::GetNativeWindow(windowPtr);
                bool renderOk = false;

#ifdef _WIN32
                renderOk = swarm::InitRenderContextWin32(&renderCtx, native.m_instance, native.m_window,
                                                         static_cast<uint32_t>(windowCtx.m_width),
                                                         static_cast<uint32_t>(windowCtx.m_height));
#elif defined(__linux__)
                switch (native.type_)
                {
                    case terra::NativeWindowType::X11:
                        renderOk = swarm::InitRenderContextX11(renderCtx, native.x11Display_, native.x11Window_,
                                                               static_cast<uint32_t>(windowCtx.m_width),
                                                               static_cast<uint32_t>(windowCtx.m_height));
                        break;
                    case terra::NativeWindowType::WAYLAND:
                        renderOk = swarm::InitRenderContextWayland(renderCtx, native.wlDisplay_, native.wlSurface_,
                                                                   static_cast<uint32_t>(windowCtx.m_width),
                                                                   static_cast<uint32_t>(windowCtx.m_height));
                        break;
                }
#endif

                if (!renderOk)
                {
                    hive::LogError(LOG_ENGINE, "Failed to create render context");
                    swarm::ShutdownSystem();
                    terra::ShutdownWindowContext(windowPtr);
                    terra::ShutdownSystem();
                    moduleRegistry.ShutdownModules();
                    return 1;
                }

                swarm::SetupGraphicPipeline(renderCtx);
                ctx.m_renderContext = &renderCtx;
                rendererActive = true;
            }
#endif
        }
#else
        (void)graphical;
#endif

        // ---- Setup callback ----
        if (callbacks.m_onSetup != nullptr)
        {
            if (!callbacks.m_onSetup(ctx, callbacks.m_userData))
            {
#if HIVE_FEATURE_GLFW
                if (graphical)
                {
#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                    if (rendererActive)
                    {
                        swarm::ShutdownRenderContext(renderCtx);
                        swarm::ShutdownSystem();
                    }
#endif
                    terra::ShutdownWindowContext(windowPtr);
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
            while (!terra::ShouldWindowClose(windowPtr) && app.IsRunning())
            {
                HIVE_PROFILE_SCOPE_N("Frame");
                terra::PollEvents();
                antennae::UpdateInput(world, windowPtr);

                if (config.m_autoTick)
                    app.Tick();

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                if (rendererActive)
                    swarm::BeginFrame(&renderCtx);
#endif

                if (callbacks.m_onFrame != nullptr)
                    callbacks.m_onFrame(ctx, callbacks.m_userData);

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                if (rendererActive)
                    swarm::EndFrame(&renderCtx);
#endif
            }
        }
        else
#endif
        {
            while (app.IsRunning())
            {
                HIVE_PROFILE_SCOPE_N("Frame");
                if (config.m_autoTick)
                    app.Tick();

                if (callbacks.m_onFrame != nullptr)
                    callbacks.m_onFrame(ctx, callbacks.m_userData);
            }
        }

        // ---- Shutdown callback ----
        if (callbacks.m_onShutdown != nullptr)
            callbacks.m_onShutdown(ctx, callbacks.m_userData);

        // ---- Cleanup ----
#if HIVE_FEATURE_GLFW
        if (graphical)
        {
#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
            if (rendererActive)
            {
                swarm::ShutdownRenderContext(renderCtx);
                swarm::ShutdownSystem();
            }
#endif
            terra::ShutdownWindowContext(windowPtr);
            terra::ShutdownSystem();
        }
#endif
        moduleRegistry.ShutdownModules();
        return 0;
    }

} // namespace waggle
