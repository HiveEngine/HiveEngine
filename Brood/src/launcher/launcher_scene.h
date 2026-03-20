#pragma once

#include <hive/hive_config.h>

#if HIVE_MODE_EDITOR

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

#include <waggle/engine_runner.h>

#include <brood/launcher/launcher_types.h>
#include <filesystem>
#include <launcher/launcher_platform.h>

namespace queen
{
    class World;
    template <size_t N> class ComponentRegistry;
} // namespace queen

namespace waggle
{
    class ProjectManager;
} // namespace waggle

namespace brood::launcher
{

    void RegisterSceneComponentTypes(LauncherState& state);
    void ResetSceneEditorState(LauncherState& state);
    void ClearWorldEntities(queen::World& world);

    std::filesystem::path GetProjectAssetsDirectory(const LauncherState& state);
    std::filesystem::path CanonicalizePathForComparison(const std::filesystem::path& path);
    bool IsPathWithinDirectory(const std::filesystem::path& path, const std::filesystem::path& directory);
    std::filesystem::path ResolveScenePath(const waggle::ProjectManager& projectManager, wax::StringView relativePath);
    wax::String MakeSceneRelativePath(const waggle::ProjectManager& projectManager,
                                      const std::filesystem::path& scenePath);

    void ResetSceneRecoveryPrompt(LauncherState& state);
    std::filesystem::path GetSceneRecoveryDirectory(const LauncherState& state);
    std::filesystem::path GetSceneRecoveryScenePath(const LauncherState& state);
    std::filesystem::path GetSceneRecoveryMetadataPath(const LauncherState& state);

    bool WriteTextFile(const std::filesystem::path& path, const wax::String& content);
    bool ReadTextFile(const std::filesystem::path& path, wax::String* content);

    wax::String GetSceneRecoverySourceRelative(LauncherState& state);
    bool WriteSceneRecoveryMetadata(LauncherState& state, const wax::String& sceneRelative);
    bool LoadSceneRecoveryMetadata(LauncherState& state, wax::String* sceneRelative);
    void ClearSceneRecoveryArtifacts(LauncherState& state);
    void QueueSceneRecoveryPrompt(LauncherState& state);

    std::filesystem::path NormalizeScenePath(const LauncherState& state, std::filesystem::path scenePath,
                                             bool allowImplicitExtension);
    bool ResolveSceneAssetPath(const LauncherState& state, const std::filesystem::path& rawScenePath,
                               bool requireExisting, std::filesystem::path* resolvedScenePath, wax::String* error);
    std::filesystem::path GetPreferredScenePath(LauncherState& state);

    bool PickScenePath(LauncherState& state, NativePathPickerMode mode, std::filesystem::path* selectedPath);
    bool LoadEditorSceneAsset(waggle::EngineContext& ctx, LauncherState& state,
                              const std::filesystem::path& rawScenePath);
    bool SaveEditorSceneToPath(waggle::EngineContext& ctx, LauncherState& state,
                               const std::filesystem::path& rawScenePath);
    bool SaveSceneRecoverySnapshot(waggle::EngineContext& ctx, LauncherState& state);
    wax::String CaptureSceneBackup(queen::World& world, const queen::ComponentRegistry<256>& registry);
    bool RestoreSceneBackup(queen::World& world, const queen::ComponentRegistry<256>& registry,
                            const wax::String& backupScene);
    bool LoadEditorScene(waggle::EngineContext& ctx, LauncherState& state, const std::filesystem::path& scenePath);

    bool SaveEditorScene(waggle::EngineContext& ctx, LauncherState& state);
    bool SaveEditorSceneAs(waggle::EngineContext& ctx, LauncherState& state);
    bool ReloadEditorScene(waggle::EngineContext& ctx, LauncherState& state);
    void NewEditorScene(waggle::EngineContext& ctx, LauncherState& state);

    [[maybe_unused]] const char* DescribePendingEditorAction(PendingEditorAction action) noexcept;
    void ExecutePendingEditorAction(waggle::EngineContext& ctx, LauncherState& state);
    void RequestPendingEditorAction(waggle::EngineContext& ctx, LauncherState& state, PendingEditorAction action,
                                    const std::filesystem::path& pendingScenePath = {});

    bool OpenEditorSceneWithPicker(waggle::EngineContext& ctx, LauncherState& state);

    [[maybe_unused]] bool RestoreSceneRecovery(waggle::EngineContext& ctx, LauncherState& state);
    [[maybe_unused]] void UpdateSceneAutosave(waggle::EngineContext& ctx, LauncherState& state);

} // namespace brood::launcher

#endif
