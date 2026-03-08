#include <hive/core/module.h>
#include <hive/core/moduleregistry.h>

#include <waggle/engine_runner.h>
#include <waggle/time.h>

#include <larvae/larvae.h>

#include <memory>

namespace
{
    struct ModuleLifecycleState
    {
        int configure_calls{0};
        int initialize_calls{0};
        int shutdown_calls{0};
        int setup_calls{0};
        int shutdown_callback_calls{0};
    };

    ModuleLifecycleState* g_moduleLifecycleState{nullptr};

    class LifecycleSmokeModule final : public hive::Module
    {
    public:
        [[nodiscard]] const char* GetName() const override
        {
            return "LifecycleSmokeModule";
        }

    protected:
        void DoConfigure([[maybe_unused]] hive::ModuleContext& context) override
        {
            if (g_moduleLifecycleState != nullptr)
            {
                ++g_moduleLifecycleState->configure_calls;
            }
        }

        void DoInitialize() override
        {
            if (g_moduleLifecycleState != nullptr)
            {
                ++g_moduleLifecycleState->initialize_calls;
            }
        }

        void DoShutdown() override
        {
            if (g_moduleLifecycleState != nullptr)
            {
                ++g_moduleLifecycleState->shutdown_calls;
            }
        }
    };

    void RegisterLifecycleSmokeModule()
    {
        hive::ModuleRegistry::GetInstance().RegisterModule(
            []() -> std::unique_ptr<hive::Module> { return std::make_unique<LifecycleSmokeModule>(); });
    }

    auto t_headless_no_callbacks = larvae::RegisterTest("WaggleEngineRunner", "headless_immediate_stop", []() {
        waggle::EngineConfig config{};
        config.m_mode = waggle::EngineMode::HEADLESS;

        waggle::EngineCallbacks callbacks{};
        callbacks.m_onSetup = [](waggle::EngineContext& ctx, void*) -> bool {
            ctx.m_app->RequestStop();
            return true;
        };

        int result = waggle::Run(config, callbacks);
        larvae::AssertEqual(result, 0);
    });

    auto t_headless_with_setup = larvae::RegisterTest("WaggleEngineRunner", "headless_setup_called", []() {
        struct State
        {
            bool setup_called{false};
            queen::World* world{nullptr};
        };
        State state;

        waggle::EngineConfig config{};
        config.m_mode = waggle::EngineMode::HEADLESS;

        waggle::EngineCallbacks callbacks{};
        callbacks.m_userData = &state;
        callbacks.m_onSetup = [](waggle::EngineContext& ctx, void* ud) -> bool {
            auto* s = static_cast<State*>(ud);
            s->setup_called = true;
            s->world = ctx.m_world;
            ctx.m_app->RequestStop();
            return true;
        };

        int result = waggle::Run(config, callbacks);
        larvae::AssertEqual(result, 0);
        larvae::AssertTrue(state.setup_called);
        larvae::AssertTrue(state.world != nullptr);
    });

    auto t_headless_setup_failure =
        larvae::RegisterTest("WaggleEngineRunner", "headless_setup_failure_returns_1", []() {
            waggle::EngineConfig config{};
            config.m_mode = waggle::EngineMode::HEADLESS;

            waggle::EngineCallbacks callbacks{};
            callbacks.m_onSetup = [](waggle::EngineContext&, void*) -> bool {
                return false;
            };

            int result = waggle::Run(config, callbacks);
            larvae::AssertEqual(result, 1);
        });

    auto t_headless_shutdown_called = larvae::RegisterTest("WaggleEngineRunner", "headless_shutdown_called", []() {
        struct State
        {
            bool shutdown_called{false};
        };
        State state;

        waggle::EngineConfig config{};
        config.m_mode = waggle::EngineMode::HEADLESS;

        waggle::EngineCallbacks callbacks{};
        callbacks.m_userData = &state;
        callbacks.m_onSetup = [](waggle::EngineContext& ctx, void*) -> bool {
            ctx.m_app->RequestStop();
            return true;
        };
        callbacks.m_onShutdown = [](waggle::EngineContext&, void* ud) {
            auto* s = static_cast<State*>(ud);
            s->shutdown_called = true;
        };

        int result = waggle::Run(config, callbacks);
        larvae::AssertEqual(result, 0);
        larvae::AssertTrue(state.shutdown_called);
    });

    auto t_headless_module_lifecycle_smoke =
        larvae::RegisterTest("WaggleEngineRunner", "headless_module_lifecycle_smoke", []() {
            ModuleLifecycleState state{};
            g_moduleLifecycleState = &state;

            waggle::EngineConfig config{};
            config.m_mode = waggle::EngineMode::HEADLESS;

            waggle::EngineCallbacks callbacks{};
            callbacks.m_onRegisterModules = RegisterLifecycleSmokeModule;
            callbacks.m_userData = &state;
            callbacks.m_onSetup = [](waggle::EngineContext& ctx, void* ud) -> bool {
                auto* s = static_cast<ModuleLifecycleState*>(ud);
                ++s->setup_calls;
                ctx.m_app->RequestStop();
                return true;
            };
            callbacks.m_onShutdown = [](waggle::EngineContext&, void* ud) {
                auto* s = static_cast<ModuleLifecycleState*>(ud);
                ++s->shutdown_callback_calls;
            };

            int result = waggle::Run(config, callbacks);
            g_moduleLifecycleState = nullptr;

            larvae::AssertEqual(result, 0);
            larvae::AssertEqual(state.configure_calls, 1);
            larvae::AssertEqual(state.initialize_calls, 1);
            larvae::AssertEqual(state.shutdown_calls, 1);
            larvae::AssertEqual(state.setup_calls, 1);
            larvae::AssertEqual(state.shutdown_callback_calls, 1);
        });

    auto t_headless_setup_failure_cleans_modules =
        larvae::RegisterTest("WaggleEngineRunner", "headless_setup_failure_cleans_modules", []() {
            ModuleLifecycleState state{};
            g_moduleLifecycleState = &state;

            waggle::EngineConfig config{};
            config.m_mode = waggle::EngineMode::HEADLESS;

            waggle::EngineCallbacks callbacks{};
            callbacks.m_onRegisterModules = RegisterLifecycleSmokeModule;
            callbacks.m_userData = &state;
            callbacks.m_onSetup = [](waggle::EngineContext&, void* ud) -> bool {
                auto* s = static_cast<ModuleLifecycleState*>(ud);
                ++s->setup_calls;
                return false;
            };
            callbacks.m_onShutdown = [](waggle::EngineContext&, void* ud) {
                auto* s = static_cast<ModuleLifecycleState*>(ud);
                ++s->shutdown_callback_calls;
            };

            int result = waggle::Run(config, callbacks);
            g_moduleLifecycleState = nullptr;

            larvae::AssertEqual(result, 1);
            larvae::AssertEqual(state.configure_calls, 1);
            larvae::AssertEqual(state.initialize_calls, 1);
            larvae::AssertEqual(state.shutdown_calls, 1);
            larvae::AssertEqual(state.setup_calls, 1);
            larvae::AssertEqual(state.shutdown_callback_calls, 0);
        });

} // namespace
