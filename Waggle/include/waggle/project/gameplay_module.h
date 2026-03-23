#pragma once

#include <hive/platform/dynamic_library.h>

#include <wax/containers/array.h>

#include <cstdint>

namespace queen
{
    class World;
}

namespace waggle
{
    using GameplayRegisterFn = void (*)(queen::World& world);
    using GameplayUnregisterFn = void (*)(queen::World& world);
    using GameplayVersionFn = const char* (*)();
    using GameplayApiVersionFn = uint32_t (*)();
    using GameplayBuildSignatureFn = const char* (*)();

    class HIVE_API GameplayModule
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
        void Abandon() noexcept;

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
        GameplayApiVersionFn m_apiVersionFn{nullptr};
        GameplayBuildSignatureFn m_buildSignatureFn{nullptr};
        bool m_registered{false};
        wax::Array<char, 256> m_errorBuf{};
    };
} // namespace waggle
