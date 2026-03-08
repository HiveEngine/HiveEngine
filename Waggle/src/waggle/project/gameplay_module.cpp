#include <hive/core/log.h>

#include <queen/world/world.h>

#include <waggle/project/gameplay_module.h>

#include <utility>

namespace
{

    static const hive::LogCategory LOG_GAMEPLAY{"Waggle.GameplayModule"};

} // namespace

namespace waggle
{

    GameplayModule::~GameplayModule()
    {
        Unload();
    }

    GameplayModule::GameplayModule(GameplayModule&& other) noexcept
        : m_lib{std::move(other.m_lib)}
        , m_registerFn{other.m_registerFn}
        , m_unregisterFn{other.m_unregisterFn}
        , m_versionFn{other.m_versionFn}
        , m_registered{other.m_registered}
    {
        other.m_registerFn = nullptr;
        other.m_unregisterFn = nullptr;
        other.m_versionFn = nullptr;
        other.m_registered = false;
    }

    GameplayModule& GameplayModule::operator=(GameplayModule&& other) noexcept
    {
        if (this != &other)
        {
            Unload();
            m_lib = std::move(other.m_lib);
            m_registerFn = other.m_registerFn;
            m_unregisterFn = other.m_unregisterFn;
            m_versionFn = other.m_versionFn;
            m_registered = other.m_registered;
            other.m_registerFn = nullptr;
            other.m_unregisterFn = nullptr;
            other.m_versionFn = nullptr;
            other.m_registered = false;
        }
        return *this;
    }

    bool GameplayModule::Load(const char* dllPath)
    {
        Unload();

        if (!m_lib.Load(dllPath))
        {
            hive::LogError(LOG_GAMEPLAY, "Failed to load gameplay DLL: {}", m_lib.GetError());
            return false;
        }

        m_registerFn = m_lib.GetFunction<GameplayRegisterFn>("HiveGameplayRegister");
        if (m_registerFn == nullptr)
        {
            hive::LogError(LOG_GAMEPLAY, "DLL missing required symbol HiveGameplayRegister");
            m_lib.Unload();
            return false;
        }

        m_unregisterFn = m_lib.GetFunction<GameplayUnregisterFn>("HiveGameplayUnregister");
        m_versionFn = m_lib.GetFunction<GameplayVersionFn>("HiveGameplayVersion");

        hive::LogInfo(LOG_GAMEPLAY, "Gameplay module loaded (version: {})",
                      m_versionFn != nullptr ? m_versionFn() : "unknown");
        return true;
    }

    void GameplayModule::Unload()
    {
        m_registerFn = nullptr;
        m_unregisterFn = nullptr;
        m_versionFn = nullptr;
        m_registered = false;
        m_lib.Unload();
    }

    bool GameplayModule::Register(queen::World& world)
    {
        if (m_registerFn == nullptr)
            return false;

        m_registerFn(world);
        world.InvalidateScheduler();
        m_registered = true;
        return true;
    }

    void GameplayModule::Unregister(queen::World& world)
    {
        if (!m_registered)
            return;

        if (m_unregisterFn != nullptr)
            m_unregisterFn(world);

        m_registered = false;
    }

    bool GameplayModule::Reload(const char* dllPath, queen::World& world)
    {
        Unregister(world);
        Unload();

        if (!Load(dllPath))
            return false;

        return Register(world);
    }

    bool GameplayModule::IsLoaded() const noexcept
    {
        return m_lib.IsLoaded();
    }

    bool GameplayModule::IsRegistered() const noexcept
    {
        return m_registered;
    }

    const char* GameplayModule::Version() const noexcept
    {
        return m_versionFn != nullptr ? m_versionFn() : "";
    }

    const char* GameplayModule::GetError() const noexcept
    {
        return m_lib.GetError();
    }

} // namespace waggle
