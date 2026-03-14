#include <hive/core/log.h>
#include <hive/core/moduleregistry.h>
#include <hive/profiling/profiler.h>

#include <waggle/engine_runner.h>

#include <terra/terra.h>
#include <terra/window_context.h>

#include <antennae/input.h>
#include <antennae/keyboard.h>

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
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

            terra::SetWindowTitle(&m_windowContext, config.m_windowTitle);
            terra::SetWindowSize(&m_windowContext, static_cast<int>(config.m_windowWidth),
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

            m_moduleRegistry.CreateModules();
            m_moduleRegistry.ConfigureModules();
            m_moduleRegistry.InitModules();
            m_modulesInitialized = true;
            return true;
        }

        [[nodiscard]] bool InitializeRuntime()
        {
            if (!IsGraphical())
            {
                return true;
            }

            if (!InitializeWindow())
            {
                return false;
            }

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
            if (m_config.m_autoRenderer && !InitializeRenderer())
            {
                return false;
            }
#endif

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

            if (!terra::InitWindowContext(&m_windowContext))
            {
                hive::LogError(LOG_ENGINE, "Failed to create window");
                return false;
            }

            m_windowInitialized = true;
            m_context.m_window = &m_windowContext;
            return true;
        }

#if (HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12)
        [[nodiscard]] bool InitializeRenderer()
        {
            if (!swarm::InitSystem())
            {
                hive::LogError(LOG_ENGINE, "Failed to initialize Swarm");
                return false;
            }

            m_rendererSystemInitialized = true;

            terra::NativeWindow native = terra::GetNativeWindow(&m_windowContext);
            bool renderOk = false;

#ifdef _WIN32
            renderOk = swarm::InitRenderContextWin32(&m_renderContext, native.m_instance, native.m_window,
                                                     static_cast<uint32_t>(terra::GetWindowWidth(&m_windowContext)),
                                                     static_cast<uint32_t>(terra::GetWindowHeight(&m_windowContext)));
#elif defined(__linux__)
            switch (native.type_)
            {
                case terra::NativeWindowType::X11:
                    renderOk =
                        swarm::InitRenderContextX11(m_renderContext, native.x11Display_, native.x11Window_,
                                                    static_cast<uint32_t>(terra::GetWindowWidth(&m_windowContext)),
                                                    static_cast<uint32_t>(terra::GetWindowHeight(&m_windowContext)));
                    break;
                case terra::NativeWindowType::WAYLAND:
                    renderOk = swarm::InitRenderContextWayland(
                        m_renderContext, native.wlDisplay_, native.wlSurface_,
                        static_cast<uint32_t>(terra::GetWindowWidth(&m_windowContext)),
                        static_cast<uint32_t>(terra::GetWindowHeight(&m_windowContext)));
                    break;
            }
#endif

            if (!renderOk)
            {
                hive::LogError(LOG_ENGINE, "Failed to create render context");
                return false;
            }

            swarm::SetupGraphicPipeline(m_renderContext);
            m_rendererInitialized = true;
            m_context.m_renderContext = &m_renderContext;
            return true;
        }
#endif

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
            if (IsGraphical())
            {
                RunGraphicalLoop();
                return;
            }

            RunHeadlessLoop();
        }

        void RunGraphicalLoop()
        {
            while (!terra::ShouldWindowClose(&m_windowContext) && m_app.IsRunning())
            {
                HIVE_PROFILE_SCOPE_N("Frame");
                terra::PollEvents(&m_windowContext);
                antennae::UpdateInput(m_app.GetWorld(), &m_windowContext);

                if (m_config.m_autoTick)
                {
                    m_app.Tick();
                }

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                if (m_rendererInitialized)
                {
                    swarm::BeginFrame(&m_renderContext);
                }
#endif

                if (m_callbacks.m_onFrame != nullptr)
                {
                    m_callbacks.m_onFrame(m_context, m_callbacks.m_userData);
                }

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                if (m_rendererInitialized)
                {
                    swarm::EndFrame(&m_renderContext);
                }
#endif
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
#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
            if (m_rendererInitialized)
            {
                swarm::ShutdownRenderContext(m_renderContext);
                m_rendererInitialized = false;
            }
            m_context.m_renderContext = nullptr;

            if (m_rendererSystemInitialized)
            {
                swarm::ShutdownSystem();
                m_rendererSystemInitialized = false;
            }
#endif

            if (m_windowInitialized)
            {
                terra::ShutdownWindowContext(&m_windowContext);
                m_windowInitialized = false;
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

        terra::WindowContext m_windowContext{};
        bool m_windowSystemInitialized{false};
        bool m_windowInitialized{false};

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
        swarm::RenderContext m_renderContext{};
        bool m_rendererSystemInitialized{false};
        bool m_rendererInitialized{false};
#endif
    };
} // namespace

namespace waggle
{

    int Run(const EngineConfig& config, const EngineCallbacks& callbacks)
    {
        EngineSession session{config, callbacks};
        return session.Run();
    }

} // namespace waggle
