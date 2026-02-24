#pragma once

#include <hive/platform/dynamic_library.h>

namespace queen { class World; }

namespace waggle
{
    using GameplayRegisterFn   = void(*)(queen::World& world);
    using GameplayUnregisterFn = void(*)(queen::World& world);
    using GameplayVersionFn    = const char*(*)();

    class GameplayModule
    {
    public:
        GameplayModule() = default;
        ~GameplayModule();

        GameplayModule(const GameplayModule&) = delete;
        GameplayModule& operator=(const GameplayModule&) = delete;
        GameplayModule(GameplayModule&&) noexcept;
        GameplayModule& operator=(GameplayModule&&) noexcept;

        [[nodiscard]] bool Load(const char* dll_path);
        void Unload();

        [[nodiscard]] bool Register(queen::World& world);
        void Unregister(queen::World& world);
        [[nodiscard]] bool Reload(const char* dll_path, queen::World& world);

        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] bool IsRegistered() const noexcept;
        [[nodiscard]] const char* Version() const noexcept;
        [[nodiscard]] const char* GetError() const noexcept;

    private:
        hive::DynamicLibrary lib_;
        GameplayRegisterFn register_fn_{nullptr};
        GameplayUnregisterFn unregister_fn_{nullptr};
        GameplayVersionFn version_fn_{nullptr};
        bool registered_{false};
    };
}
