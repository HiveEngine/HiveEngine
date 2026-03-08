#include <waggle/engine_runner.h>
#include <waggle/time.h>

#include <larvae/larvae.h>

namespace
{

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

} // namespace
