#include <waggle/project/gameplay_module.h>

#include <queen/world/world.h>
#include <hive/core/log.h>

#include <utility>

namespace
{

static const hive::LogCategory LogGameplay{"Waggle.GameplayModule"};

} // namespace

namespace waggle
{

GameplayModule::~GameplayModule()
{
    Unload();
}

GameplayModule::GameplayModule(GameplayModule&& other) noexcept
    : lib_{std::move(other.lib_)}
    , register_fn_{other.register_fn_}
    , unregister_fn_{other.unregister_fn_}
    , version_fn_{other.version_fn_}
    , registered_{other.registered_}
{
    other.register_fn_ = nullptr;
    other.unregister_fn_ = nullptr;
    other.version_fn_ = nullptr;
    other.registered_ = false;
}

GameplayModule& GameplayModule::operator=(GameplayModule&& other) noexcept
{
    if (this != &other)
    {
        Unload();
        lib_ = std::move(other.lib_);
        register_fn_ = other.register_fn_;
        unregister_fn_ = other.unregister_fn_;
        version_fn_ = other.version_fn_;
        registered_ = other.registered_;
        other.register_fn_ = nullptr;
        other.unregister_fn_ = nullptr;
        other.version_fn_ = nullptr;
        other.registered_ = false;
    }
    return *this;
}

bool GameplayModule::Load(const char* dll_path)
{
    Unload();

    if (!lib_.Load(dll_path))
    {
        hive::LogError(LogGameplay, "Failed to load gameplay DLL: {}", lib_.GetError());
        return false;
    }

    register_fn_ = lib_.GetFunction<GameplayRegisterFn>("HiveGameplayRegister");
    if (!register_fn_)
    {
        hive::LogError(LogGameplay, "DLL missing required symbol HiveGameplayRegister");
        lib_.Unload();
        return false;
    }

    unregister_fn_ = lib_.GetFunction<GameplayUnregisterFn>("HiveGameplayUnregister");
    version_fn_ = lib_.GetFunction<GameplayVersionFn>("HiveGameplayVersion");

    hive::LogInfo(LogGameplay, "Gameplay module loaded (version: {})",
                  version_fn_ ? version_fn_() : "unknown");
    return true;
}

void GameplayModule::Unload()
{
    register_fn_ = nullptr;
    unregister_fn_ = nullptr;
    version_fn_ = nullptr;
    registered_ = false;
    lib_.Unload();
}

bool GameplayModule::Register(queen::World& world)
{
    if (!register_fn_)
        return false;

    register_fn_(world);
    world.InvalidateScheduler();
    registered_ = true;
    return true;
}

void GameplayModule::Unregister(queen::World& world)
{
    if (!registered_)
        return;

    if (unregister_fn_)
        unregister_fn_(world);

    registered_ = false;
}

bool GameplayModule::Reload(const char* dll_path, queen::World& world)
{
    Unregister(world);
    Unload();

    if (!Load(dll_path))
        return false;

    return Register(world);
}

bool GameplayModule::IsLoaded() const noexcept
{
    return lib_.IsLoaded();
}

bool GameplayModule::IsRegistered() const noexcept
{
    return registered_;
}

const char* GameplayModule::Version() const noexcept
{
    return version_fn_ ? version_fn_() : "";
}

const char* GameplayModule::GetError() const noexcept
{
    return lib_.GetError();
}

} // namespace waggle
