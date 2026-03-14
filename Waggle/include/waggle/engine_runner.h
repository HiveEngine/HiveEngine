#pragma once

#include <waggle/app.h>

#include <cstdint>

namespace terra
{
    struct WindowContext;
}

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
namespace swarm
{
    struct RenderContext;
}
#endif

namespace waggle
{
    enum class EngineMode : uint8_t
    {
        GAME,
        EDITOR,
        HEADLESS
    };

    struct EngineConfig
    {
        const char* m_windowTitle{"HiveEngine"};
        uint32_t m_windowWidth{1280};
        uint32_t m_windowHeight{720};
        EngineMode m_mode{EngineMode::GAME};
        bool m_autoTick{true};
        bool m_autoRenderer{true};
        bool m_autoSystems{true};
        AppConfig m_app{};
    };

    struct EngineContext
    {
        waggle::App* m_app{nullptr};
        queen::World* m_world{nullptr};
        terra::WindowContext* m_window{nullptr};

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
        swarm::RenderContext* m_renderContext{nullptr};
#endif
    };

    struct EngineCallbacks
    {
        using RegisterModulesFn = void (*)();
        using SetupFn = bool (*)(EngineContext& ctx, void* userData);
        using FrameFn = void (*)(EngineContext& ctx, void* userData);
        using ShutdownFn = void (*)(EngineContext& ctx, void* userData);

        RegisterModulesFn m_onRegisterModules{nullptr};
        SetupFn m_onSetup{nullptr};
        FrameFn m_onFrame{nullptr};
        ShutdownFn m_onShutdown{nullptr};

        void* m_userData{nullptr};
    };

    // Main engine entry point.
    // Manages the full lifecycle: modules -> window -> device -> swapchain -> loop -> cleanup.
    // Returns 0 on success.
    int Run(const EngineConfig& config, const EngineCallbacks& callbacks);
} // namespace waggle
