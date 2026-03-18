#include <hive/core/log.h>
#include <hive/core/moduleregistry.h>
#include <hive/profiling/profiler.h>

#include <waggle/engine_runner.h>

#include <swarm/swarm.h>

#include <terra/terra.h>

#include <antennae/input.h>
#include <antennae/keyboard.h>

static const hive::LogCategory LOG_ENGINE{"Waggle.EngineRunner"};

namespace waggle
{
    extern void RegisterModule();
}
namespace
{
    class EngineSession
    {
    public:
        EngineSession(const waggle::EngineConfig& config, const waggle::EngineCallbacks& callbacks)
            : m_config{config}
            , m_callbacks{callbacks}
            , m_app{config.m_app}
        {
            m_context.m_app = &m_app;
            m_context.m_world = &m_app.GetWorld();
            m_app.GetWorld().InsertResource(waggle::RuntimeContext{config.m_mode});

            terra::SetWindowTitle(m_windowContext, config.m_windowTitle);
            terra::SetWindowSize(m_windowContext, static_cast<int>(config.m_windowWidth),
                                 static_cast<int>(config.m_windowHeight));
        }

        ~EngineSession()
        {
            InvokeShutdownCallback();
            Cleanup();
        }

        int Run()
        {
            if (!InitializeModules())
            {
                return 1;
            }

            if (!InitializeRuntime())
            {
                return 1;
            }

            if (!RunSetupCallback())
            {
                return 1;
            }

            RunMainLoop();
            return 0;
        }

    private:
        [[nodiscard]] bool InitializeModules()
        {
            if (m_callbacks.m_onRegisterModules != nullptr)
            {
                m_callbacks.m_onRegisterModules();
            }

            waggle::RegisterModule();
            m_moduleRegistry.CreateModules();
            m_moduleRegistry.ConfigureModules();
            m_moduleRegistry.InitModules();
            m_modulesInitialized = true;
            return true;
        }

        [[nodiscard]] bool InitializeRuntime()
        {
            if (!IsGraphical() || m_config.m_deferWindow)
            {
                return true;
            }

            if (!InitializeWindow())
            {
                return false;
            }

            if (m_config.m_autoRenderer && !InitializeRenderer())
            {
                return false;
            }

            return true;
        }

        [[nodiscard]] bool InitializeWindow()
        {
            if (!terra::InitSystem())
            {
                hive::LogError(LOG_ENGINE, "Failed to initialize windowing backend");
                return false;
            }

            m_windowSystemInitialized = true;

            m_windowContext = terra::CreateWindowContext(m_config.m_windowTitle, static_cast<int>(m_config.m_windowWidth),
                                              static_cast<int>(m_config.m_windowHeight));

            if (m_windowContext == nullptr)
            {
                hive::LogError(LOG_ENGINE, "Failed to create window");
                return false;
            }

            m_windowInitialized = true;
            m_context.m_window = m_windowContext;
            return true;
        }

        [[nodiscard]] bool InitializeRenderer()
        {
            if (!swarm::InitSystem())
            {
                hive::LogError(LOG_ENGINE, "Failed to initialize Swarm");
                return false;
            }

            m_rendererSystemInitialized = true;

            m_renderContext = swarm::CreateRenderContext(m_windowContext);
            if (m_renderContext == nullptr)
            {
                hive::LogError(LOG_ENGINE, "Failed to create render context");
                return false;
            }

            swarm::SetupGraphicPipeline(*m_renderContext);
            m_rendererInitialized = true;
            m_context.m_renderContext = m_renderContext;
            return true;
        }

        [[nodiscard]] bool RunSetupCallback()
        {
            if (m_callbacks.m_onSetup != nullptr && !m_callbacks.m_onSetup(m_context, m_callbacks.m_userData))
            {
                return false;
            }

            m_setupCompleted = true;
            return true;
        }

        void RunMainLoop()
        {
            if (IsGraphical() && m_windowInitialized)
            {
                RunGraphicalLoop();
                return;
            }

            if (IsGraphical() && m_config.m_deferWindow)
            {
                RunDeferredLoop();
                return;
            }

            RunHeadlessLoop();
        }

        void RunDeferredLoop()
        {
            while (m_app.IsRunning())
            {
                HIVE_PROFILE_SCOPE_N("Frame");

                if (m_context.m_window != nullptr)
                {
                    terra::PollEvents(m_context.m_window);
                    antennae::UpdateInput(m_app.GetWorld(), m_context.m_window);

                    if (terra::ShouldWindowClose(m_context.m_window))
                        break;
                }

                if (m_config.m_autoTick)
                {
                    m_app.Tick();
                }

                if (m_context.m_renderContext != nullptr)
                {
                    swarm::BeginFrame(m_context.m_renderContext);
                }

                if (m_callbacks.m_onFrame != nullptr)
                {
                    m_callbacks.m_onFrame(m_context, m_callbacks.m_userData);
                }

                if (m_context.m_renderContext != nullptr)
                {
                    swarm::EndFrame(m_context.m_renderContext);
                }
            }
        }

        void RunGraphicalLoop()
        {
            while (!terra::ShouldWindowClose(m_windowContext) && m_app.IsRunning())
            {
                HIVE_PROFILE_SCOPE_N("Frame");
                terra::PollEvents(m_windowContext);
                antennae::UpdateInput(m_app.GetWorld(), m_windowContext);

                if (m_config.m_autoTick)
                {
                    m_app.Tick();
                }

                if (m_rendererInitialized)
                {
                    swarm::BeginFrame(m_renderContext);
                }

                if (m_callbacks.m_onFrame != nullptr)
                {
                    m_callbacks.m_onFrame(m_context, m_callbacks.m_userData);
                }

                if (m_rendererInitialized)
                {
                    swarm::EndFrame(m_renderContext);
                }
            }
        }

        void RunHeadlessLoop()
        {
            while (m_app.IsRunning())
            {
                HIVE_PROFILE_SCOPE_N("Frame");

                if (m_config.m_autoTick)
                {
                    m_app.Tick();
                }

                if (m_callbacks.m_onFrame != nullptr)
                {
                    m_callbacks.m_onFrame(m_context, m_callbacks.m_userData);
                }
            }
        }

        void InvokeShutdownCallback()
        {
            if (!m_setupCompleted || m_shutdownCallbackInvoked || m_callbacks.m_onShutdown == nullptr)
            {
                return;
            }

            m_callbacks.m_onShutdown(m_context, m_callbacks.m_userData);
            m_shutdownCallbackInvoked = true;
        }

        void Cleanup()
        {
            if (m_rendererInitialized)
            {
                swarm::DestroyRenderContext(m_renderContext);
                m_rendererInitialized = false;
            }
            else if (m_context.m_renderContext != nullptr)
            {
                swarm::DestroyRenderContext(m_context.m_renderContext);
                swarm::ShutdownSystem();
            }
            m_context.m_renderContext = nullptr;

            if (m_rendererSystemInitialized)
            {
                swarm::ShutdownSystem();
                m_rendererSystemInitialized = false;
            }

            if (m_windowInitialized)
            {
                terra::DestroyWindowContext(m_windowContext);
                m_windowInitialized = false;
            }
            else if (m_context.m_window != nullptr)
            {
                terra::DestroyWindowContext(m_context.m_window);
                terra::ShutdownSystem();
            }
            m_context.m_window = nullptr;

            if (m_windowSystemInitialized)
            {
                terra::ShutdownSystem();
                m_windowSystemInitialized = false;
            }

            if (m_modulesInitialized)
            {
                m_moduleRegistry.ShutdownModules();
                m_modulesInitialized = false;
            }
        }

        [[nodiscard]] bool IsGraphical() const
        {
            return m_config.m_mode != waggle::EngineMode::HEADLESS;
        }

        const waggle::EngineConfig& m_config;
        const waggle::EngineCallbacks& m_callbacks;
        hive::ModuleRegistry m_moduleRegistry{};
        waggle::App m_app;
        waggle::EngineContext m_context{};
        bool m_modulesInitialized{false};
        bool m_setupCompleted{false};
        bool m_shutdownCallbackInvoked{false};

        terra::WindowContext *m_windowContext{nullptr};
        bool m_windowSystemInitialized{false};
        bool m_windowInitialized{false};

        swarm::RenderContext* m_renderContext{nullptr};
        bool m_rendererSystemInitialized{false};
        bool m_rendererInitialized{false};
    };
} // namespace

namespace waggle
{

    int Run(const EngineConfig& config, const EngineCallbacks& callbacks)
    {
        EngineSession session{config, callbacks};
        return session.Run();
    }

    bool CreateWindowAndRenderer(EngineContext& ctx, const char* title, uint32_t width, uint32_t height)
    {
        if (ctx.m_window != nullptr)
            return true;

        if (!terra::InitSystem())
        {
            hive::LogError(LOG_ENGINE, "Failed to initialize windowing backend");
            return false;
        }

        auto* window = terra::CreateWindowContext(title, static_cast<int>(width), static_cast<int>(height));
        if (window == nullptr)
        {
            hive::LogError(LOG_ENGINE, "Failed to create window");
            return false;
        }
        ctx.m_window = window;

        if (!swarm::InitSystem())
        {
            hive::LogError(LOG_ENGINE, "Failed to initialize Swarm");
            return false;
        }

        auto* renderCtx = swarm::CreateRenderContext(window);
        if (renderCtx == nullptr)
        {
            hive::LogError(LOG_ENGINE, "Failed to create render context");
            return false;
        }

        swarm::SetupGraphicPipeline(*renderCtx);
        ctx.m_renderContext = renderCtx;
        return true;
    }

} // namespace waggle
