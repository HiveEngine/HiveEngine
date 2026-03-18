#include <hive/core/log.h>
#include <hive/project/gameplay_api.h>

#include <queen/world/world.h>

#include <waggle/project/gameplay_module.h>

#include <cstdio>
#include <cstring>
#include <utility>

namespace
{

    static const hive::LogCategory LOG_GAMEPLAY{"Waggle.GameplayModule"};

    void CopyError(std::array<char, 256>& buffer, const char* message)
    {
        if (message == nullptr)
        {
            buffer[0] = '\0';
            return;
        }

        std::snprintf(buffer.data(), buffer.size(), "%s", message);
    }

    void SetCompatibilityError(std::array<char, 256>& buffer, const char* dllPath, uint32_t actualVersion)
    {
        std::snprintf(buffer.data(), buffer.size(),
                      "Gameplay module '%s' uses API %u but the engine requires API %u. Rebuild the project module.",
                      dllPath != nullptr ? dllPath : "<unknown>", actualVersion, HIVE_GAMEPLAY_API_VERSION);
    }

    void SetBuildSignatureError(std::array<char, 256>& buffer, const char* dllPath)
    {
        std::snprintf(buffer.data(), buffer.size(),
                      "Gameplay module '%s' was built against a different engine configuration. Rebuild the project module.",
                      dllPath != nullptr ? dllPath : "<unknown>");
    }

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
        , m_apiVersionFn{other.m_apiVersionFn}
        , m_buildSignatureFn{other.m_buildSignatureFn}
        , m_registered{other.m_registered}
        , m_errorBuf{other.m_errorBuf}
    {
        other.m_registerFn = nullptr;
        other.m_unregisterFn = nullptr;
        other.m_versionFn = nullptr;
        other.m_apiVersionFn = nullptr;
        other.m_buildSignatureFn = nullptr;
        other.m_registered = false;
        other.m_errorBuf[0] = '\0';
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
            m_apiVersionFn = other.m_apiVersionFn;
            m_buildSignatureFn = other.m_buildSignatureFn;
            m_registered = other.m_registered;
            m_errorBuf = other.m_errorBuf;
            other.m_registerFn = nullptr;
            other.m_unregisterFn = nullptr;
            other.m_versionFn = nullptr;
            other.m_apiVersionFn = nullptr;
            other.m_buildSignatureFn = nullptr;
            other.m_registered = false;
            other.m_errorBuf[0] = '\0';
        }
        return *this;
    }

    bool GameplayModule::Load(const char* dllPath)
    {
        Unload();
        m_errorBuf[0] = '\0';

        if (!m_lib.Load(dllPath))
        {
            CopyError(m_errorBuf, m_lib.GetError());
            hive::LogError(LOG_GAMEPLAY, "Failed to load gameplay DLL: {}", m_errorBuf.data());
            return false;
        }

        m_registerFn = m_lib.GetFunction<GameplayRegisterFn>("HiveGameplayRegister");
        if (m_registerFn == nullptr)
        {
            CopyError(m_errorBuf, "DLL missing required symbol HiveGameplayRegister");
            hive::LogError(LOG_GAMEPLAY, "{}", m_errorBuf.data());
            m_lib.Unload();
            m_registerFn = nullptr;
            m_unregisterFn = nullptr;
            m_versionFn = nullptr;
            m_apiVersionFn = nullptr;
            return false;
        }

        m_unregisterFn = m_lib.GetFunction<GameplayUnregisterFn>("HiveGameplayUnregister");
        m_versionFn = m_lib.GetFunction<GameplayVersionFn>("HiveGameplayVersion");
        m_apiVersionFn = m_lib.GetFunction<GameplayApiVersionFn>("HiveGameplayApiVersion");
        if (m_apiVersionFn == nullptr)
        {
            CopyError(m_errorBuf,
                      "DLL missing required symbol HiveGameplayApiVersion. Rebuild the project module.");
            hive::LogError(LOG_GAMEPLAY, "{}", m_errorBuf.data());
            m_lib.Unload();
            m_registerFn = nullptr;
            m_unregisterFn = nullptr;
            m_versionFn = nullptr;
            m_apiVersionFn = nullptr;
            m_buildSignatureFn = nullptr;
            return false;
        }

        const uint32_t apiVersion = m_apiVersionFn();
        if (apiVersion != HIVE_GAMEPLAY_API_VERSION)
        {
            SetCompatibilityError(m_errorBuf, dllPath, apiVersion);
            hive::LogError(LOG_GAMEPLAY, "{}", m_errorBuf.data());
            m_lib.Unload();
            m_registerFn = nullptr;
            m_unregisterFn = nullptr;
            m_versionFn = nullptr;
            m_apiVersionFn = nullptr;
            m_buildSignatureFn = nullptr;
            return false;
        }

        m_buildSignatureFn = m_lib.GetFunction<GameplayBuildSignatureFn>("HiveGameplayBuildSignature");
        if (m_buildSignatureFn == nullptr)
        {
            CopyError(m_errorBuf,
                      "DLL missing required symbol HiveGameplayBuildSignature. Rebuild the project module.");
            hive::LogError(LOG_GAMEPLAY, "{}", m_errorBuf.data());
            m_lib.Unload();
            m_registerFn = nullptr;
            m_unregisterFn = nullptr;
            m_versionFn = nullptr;
            m_apiVersionFn = nullptr;
            m_buildSignatureFn = nullptr;
            return false;
        }

        const char* buildSignature = m_buildSignatureFn();
        if (buildSignature == nullptr || std::strcmp(buildSignature, HIVE_GAMEPLAY_BUILD_SIGNATURE) != 0)
        {
            SetBuildSignatureError(m_errorBuf, dllPath);
            hive::LogError(LOG_GAMEPLAY, "{}", m_errorBuf.data());
            m_lib.Unload();
            m_registerFn = nullptr;
            m_unregisterFn = nullptr;
            m_versionFn = nullptr;
            m_apiVersionFn = nullptr;
            m_buildSignatureFn = nullptr;
            return false;
        }

        hive::LogInfo(LOG_GAMEPLAY, "Gameplay module loaded (version: {})",
                      m_versionFn != nullptr ? m_versionFn() : "unknown");
        return true;
    }

    void GameplayModule::Unload()
    {
        m_registerFn = nullptr;
        m_unregisterFn = nullptr;
        m_versionFn = nullptr;
        m_apiVersionFn = nullptr;
        m_buildSignatureFn = nullptr;
        m_registered = false;
        m_errorBuf[0] = '\0';
        m_lib.Unload();
    }

    void GameplayModule::Abandon() noexcept
    {
        m_registerFn = nullptr;
        m_unregisterFn = nullptr;
        m_versionFn = nullptr;
        m_apiVersionFn = nullptr;
        m_buildSignatureFn = nullptr;
        m_registered = false;
        m_errorBuf[0] = '\0';
        (void)m_lib.Detach();
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
        if (m_errorBuf[0] != '\0')
        {
            return m_errorBuf.data();
        }

        return m_lib.GetError();
    }

} // namespace waggle
