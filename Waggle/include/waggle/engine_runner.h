#pragma once

#include <waggle/app.h>
#include <cstdint>

#if HIVE_FEATURE_GLFW
namespace terra { struct WindowContext; }
#endif

// TODO: Re-add Swarm forward declarations once Diligent renderer API is migrated

namespace waggle
{
    enum class EngineMode : uint8_t
    {
        Game,
        Editor,
        Headless
    };

    struct EngineConfig
    {
        const char* window_title{"HiveEngine"};
        uint32_t window_width{1280};
        uint32_t window_height{720};
        EngineMode mode{EngineMode::Game};
        bool auto_tick{true};
        bool auto_renderer{true};
        bool auto_systems{true};
        AppConfig app{};
    };

    struct EngineContext
    {
        waggle::App*   app{nullptr};
        queen::World*  world{nullptr};

#if HIVE_FEATURE_GLFW
        terra::WindowContext* window{nullptr};
#endif

        // TODO: Re-add Swarm device/swapchain/frame_mgr once migrated to Diligent
    };

    struct EngineCallbacks
    {
        using RegisterModulesFn = void(*)();
        using SetupFn    = bool(*)(EngineContext& ctx, void* user_data);
        using FrameFn    = void(*)(EngineContext& ctx, void* user_data);
        using ShutdownFn = void(*)(EngineContext& ctx, void* user_data);

        RegisterModulesFn on_register_modules{nullptr};
        SetupFn    on_setup{nullptr};
        FrameFn    on_frame{nullptr};
        ShutdownFn on_shutdown{nullptr};

        void* user_data{nullptr};
    };

    // Main engine entry point.
    // Manages the full lifecycle: modules -> window -> device -> swapchain -> loop -> cleanup.
    // Returns 0 on success.
    int Run(const EngineConfig& config, const EngineCallbacks& callbacks);
}
