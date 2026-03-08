#pragma once

#include <hive/platform/dynamic_library.h>

namespace queen
{
    class World;
}

namespace waggle
{
    using GameplayRegisterFn = void (*)(queen::World& world);
    using GameplayUnregisterFn = void (*)(queen::World& world);
    using GameplayVersionFn = const char* (*)();

    class GameplayModule
    {
    public:
        GameplayModule() = default;
        ~GameplayModule();

        GameplayModule(const GameplayModule&) = delete;
        GameplayModule& operator=(const GameplayModule&) = delete;
        GameplayModule(GameplayModule&&) noexcept;
        GameplayModule& operator=(GameplayModule&&) noexcept;

        [[nodiscard]] bool Load(const char* dllPath);
        void Unload();

        [[nodiscard]] bool Register(queen::World& world);
        void Unregister(queen::World& world);
        [[nodiscard]] bool Reload(const char* dllPath, queen::World& world);

        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] bool IsRegistered() const noexcept;
        [[nodiscard]] const char* Version() const noexcept;
        [[nodiscard]] const char* GetError() const noexcept;

    private:
        hive::DynamicLibrary m_lib;
        GameplayRegisterFn m_registerFn{nullptr};
        GameplayUnregisterFn m_unregisterFn{nullptr};
        GameplayVersionFn m_versionFn{nullptr};
        bool m_registered{false};
    };
} // namespace waggle
