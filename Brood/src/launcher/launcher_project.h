#pragma once

#include <waggle/engine_runner.h>

#include <brood/launcher/launcher_types.h>
#include <filesystem>

namespace brood::launcher
{

    void TryLoadGameplayModule(waggle::EngineContext& ctx, LauncherState& state);
    bool OpenProject(waggle::EngineContext& ctx, LauncherState& state, const std::filesystem::path& requestedPath);

#if HIVE_MODE_EDITOR
    bool CreateProjectFromHub(waggle::EngineContext& ctx, LauncherState& state);
#endif

} // namespace brood::launcher
