#include <launcher/launcher_scene.h>

#if HIVE_MODE_EDITOR

#include <hive/core/log.h>

#include <wax/containers/string.h>
#include <wax/containers/vector.h>

#include <queen/reflect/component_registry.h>
#include <queen/reflect/world_deserializer.h>
#include <queen/reflect/world_serializer.h>
#include <queen/world/world.h>

#include <nectar/hive/hive_document.h>
#include <nectar/hive/hive_parser.h>
#include <nectar/hive/hive_writer.h>

#include <waggle/components/camera.h>
#include <waggle/components/lighting.h>
#include <waggle/components/mesh_reference.h>
#include <waggle/components/name.h>
#include <waggle/components/transform.h>
#include <waggle/project/project_manager.h>
#include <waggle/time.h>

#include <forge/scene_file.h>

#include <cstdio>
#include <memory>

namespace brood::launcher
{

    void RegisterSceneComponentTypes(LauncherState& state)
    {
        auto& registry = state.m_componentRegistry;
        if (!registry.Contains<waggle::Transform>())
            registry.Register<waggle::Transform>();
        if (!registry.Contains<waggle::Camera>())
            registry.Register<waggle::Camera>();
        if (!registry.Contains<waggle::DirectionalLight>())
            registry.Register<waggle::DirectionalLight>();
        if (!registry.Contains<waggle::AmbientLight>())
            registry.Register<waggle::AmbientLight>();
        if (!registry.Contains<waggle::Name>())
            registry.Register<waggle::Name>();
        if (!registry.Contains<waggle::MeshReference>())
            registry.Register<waggle::MeshReference>();
    }

    void ResetSceneEditorState(LauncherState& state)
    {
        state.m_selection.Clear();
        state.m_pendingEditorAction = PendingEditorAction::NONE;
        state.m_pendingScenePath.Clear();
        state.m_openUnsavedScenePopup = false;
        state.m_scenePromptError.Clear();
    }

    void ClearWorldEntities(queen::World& world)
    {
        wax::Vector<queen::Entity> roots;
        roots.Reserve(world.EntityCount());

        world.ForEachArchetype([&](auto& archetype) {
            for (uint32_t row = 0; row < archetype.EntityCount(); ++row)
            {
                const queen::Entity entity = archetype.GetEntity(row);
                if (world.GetParent(entity).IsNull())
                {
                    roots.PushBack(entity);
                }
            }
        });

        for (queen::Entity entity : roots)
        {
            if (world.IsAlive(entity))
            {
                world.DespawnRecursive(entity);
            }
        }

        wax::Vector<queen::Entity> leftovers;
        world.ForEachArchetype([&](auto& archetype) {
            for (uint32_t row = 0; row < archetype.EntityCount(); ++row)
            {
                leftovers.PushBack(archetype.GetEntity(row));
            }
        });

        for (queen::Entity entity : leftovers)
        {
            if (world.IsAlive(entity))
            {
                world.Despawn(entity);
            }
        }
    }

    std::filesystem::path GetProjectAssetsDirectory(const LauncherState& state)
    {
        if (state.m_project == nullptr)
        {
            return {};
        }

        return std::filesystem::path{state.m_project->Paths().m_assets.CStr()};
    }

    std::filesystem::path CanonicalizePathForComparison(const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
        if (ec)
        {
            normalized = std::filesystem::absolute(path, ec);
            if (ec)
            {
                normalized = path;
            }
        }

        return normalized.lexically_normal();
    }

    bool IsPathWithinDirectory(const std::filesystem::path& path, const std::filesystem::path& directory)
    {
        if (path.empty() || directory.empty())
        {
            return false;
        }

        const std::filesystem::path normalizedPath = CanonicalizePathForComparison(path);
        const std::filesystem::path normalizedDirectory = CanonicalizePathForComparison(directory);

        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(normalizedPath, normalizedDirectory, ec);
        if (ec)
        {
            return false;
        }

        const wax::String relativeString{relative.generic_string().c_str()};
        return !relativeString.IsEmpty() && relativeString != ".." && !relativeString.StartsWith("../");
    }

    std::filesystem::path ResolveScenePath(const waggle::ProjectManager& projectManager, wax::StringView relativePath)
    {
        if (relativePath.IsEmpty())
        {
            return {};
        }

        return std::filesystem::path{projectManager.Paths().m_assets.CStr()} / wax::String{relativePath}.CStr();
    }

    wax::String MakeSceneRelativePath(const waggle::ProjectManager& projectManager,
                                      const std::filesystem::path& scenePath)
    {
        std::error_code ec;
        const std::filesystem::path relative =
            std::filesystem::relative(scenePath, std::filesystem::path{projectManager.Paths().m_assets.CStr()}, ec);
        if (ec)
        {
            return wax::String{scenePath.generic_string().c_str()};
        }

        const wax::String value{relative.generic_string().c_str()};
        if (value.IsEmpty() || value.StartsWith(".."))
        {
            return wax::String{scenePath.generic_string().c_str()};
        }
        return value;
    }

    void ResetSceneRecoveryPrompt(LauncherState& state)
    {
        state.m_recoverySceneRelative.Clear();
        state.m_recoveryPromptError.Clear();
        state.m_openRecoveryScenePopup = false;
    }

    std::filesystem::path GetSceneRecoveryDirectory(const LauncherState& state)
    {
        return GetProjectAssetsDirectory(state) / ".nectar" / "editor_recovery";
    }

    std::filesystem::path GetSceneRecoveryScenePath(const LauncherState& state)
    {
        return GetSceneRecoveryDirectory(state) / "autosave.hscene";
    }

    std::filesystem::path GetSceneRecoveryMetadataPath(const LauncherState& state)
    {
        return GetSceneRecoveryDirectory(state) / "autosave.hive";
    }

    bool WriteTextFile(const std::filesystem::path& path, const wax::String& content)
    {
        FILE* file = nullptr;
#ifdef _MSC_VER
        fopen_s(&file, path.string().c_str(), "wb");
#else
        file = fopen(path.string().c_str(), "wb");
#endif
        if (file == nullptr)
        {
            return false;
        }

        const size_t written = std::fwrite(content.Data(), 1, content.Size(), file);
        std::fclose(file);
        return written == content.Size();
    }

    bool ReadTextFile(const std::filesystem::path& path, wax::String* content)
    {
        FILE* file = nullptr;
#ifdef _MSC_VER
        fopen_s(&file, path.string().c_str(), "rb");
#else
        file = fopen(path.string().c_str(), "rb");
#endif
        if (file == nullptr)
        {
            return false;
        }

        if (std::fseek(file, 0, SEEK_END) != 0)
        {
            std::fclose(file);
            return false;
        }

        const long fileSize = std::ftell(file);
        if (fileSize < 0 || std::fseek(file, 0, SEEK_SET) != 0)
        {
            std::fclose(file);
            return false;
        }

        wax::Vector<char> buffer;
        buffer.Resize(static_cast<size_t>(fileSize) + 1);
        buffer[static_cast<size_t>(fileSize)] = '\0';
        const size_t bytesRead = std::fread(buffer.Data(), 1, static_cast<size_t>(fileSize), file);
        std::fclose(file);
        if (bytesRead != static_cast<size_t>(fileSize))
        {
            return false;
        }

        *content = wax::String{wax::StringView{buffer.Data(), bytesRead}};
        return true;
    }

    wax::String GetSceneRecoverySourceRelative(LauncherState& state)
    {
        if (!state.m_currentSceneRelative.IsEmpty())
        {
            return state.m_currentSceneRelative;
        }

        if (state.m_project == nullptr)
        {
            return {};
        }

        std::filesystem::path preferredScenePath{};
        if (!state.m_currentScenePath.IsEmpty())
        {
            preferredScenePath = std::filesystem::path{state.m_currentScenePath.CStr()};
        }
        else
        {
            const wax::StringView startupScene = state.m_project->Project().StartupSceneRelative();
            preferredScenePath = !startupScene.IsEmpty() ? ResolveScenePath(*state.m_project, startupScene)
                                                         : GetProjectAssetsDirectory(state) / "scenes" / "main.hscene";
        }

        return MakeSceneRelativePath(*state.m_project, preferredScenePath);
    }

    bool WriteSceneRecoveryMetadata(LauncherState& state, const wax::String& sceneRelative)
    {
        auto& alloc = state.m_alloc.Get();
        nectar::HiveDocument document{alloc};
        document.SetValue("recovery", "scene",
                          nectar::HiveValue::MakeString(alloc, {sceneRelative.CStr(), sceneRelative.Size()}));
        const wax::String content = nectar::HiveWriter::Write(document, alloc);
        return WriteTextFile(GetSceneRecoveryMetadataPath(state), content);
    }

    bool LoadSceneRecoveryMetadata(LauncherState& state, wax::String* sceneRelative)
    {
        wax::String content{};
        if (!ReadTextFile(GetSceneRecoveryMetadataPath(state), &content))
        {
            return false;
        }

        auto& alloc = state.m_alloc.Get();
        const nectar::HiveParseResult parseResult =
            nectar::HiveParser::Parse(wax::StringView{content.CStr(), content.Size()}, alloc);
        if (!parseResult.Success())
        {
            return false;
        }

        const wax::StringView recoveryScene = parseResult.m_document.GetString("recovery", "scene");
        if (recoveryScene.IsEmpty())
        {
            return false;
        }

        *sceneRelative = wax::String{recoveryScene};
        return true;
    }

    void ClearSceneRecoveryArtifacts(LauncherState& state)
    {
        std::error_code ec;
        std::filesystem::remove(GetSceneRecoveryScenePath(state), ec);
        ec.clear();
        std::filesystem::remove(GetSceneRecoveryMetadataPath(state), ec);
        ResetSceneRecoveryPrompt(state);
    }

    void QueueSceneRecoveryPrompt(LauncherState& state)
    {
        std::error_code ec;
        const std::filesystem::path recoveryScenePath = GetSceneRecoveryScenePath(state);
        const std::filesystem::path recoveryMetadataPath = GetSceneRecoveryMetadataPath(state);
        if (!std::filesystem::exists(recoveryScenePath, ec) || ec)
        {
            return;
        }

        ec.clear();
        if (!std::filesystem::exists(recoveryMetadataPath, ec) || ec)
        {
            ClearSceneRecoveryArtifacts(state);
            return;
        }

        wax::String sceneRelative{};
        if (!LoadSceneRecoveryMetadata(state, &sceneRelative))
        {
            ClearSceneRecoveryArtifacts(state);
            return;
        }

        state.m_recoverySceneRelative = sceneRelative;
        state.m_recoveryPromptError.Clear();
        state.m_openRecoveryScenePopup = true;
    }

    std::filesystem::path NormalizeScenePath(const LauncherState& state, std::filesystem::path scenePath,
                                             bool allowImplicitExtension)
    {
        if (scenePath.empty())
        {
            return {};
        }

        if (scenePath.is_relative())
        {
            scenePath = GetProjectAssetsDirectory(state) / scenePath;
        }

        scenePath = scenePath.lexically_normal();
        if (allowImplicitExtension && scenePath.has_filename() && scenePath.extension().empty())
        {
            scenePath.replace_extension(".hscene");
        }

        return scenePath;
    }

    bool ResolveSceneAssetPath(const LauncherState& state, const std::filesystem::path& rawScenePath,
                               bool requireExisting, std::filesystem::path* resolvedScenePath, wax::String* error)
    {
        if (state.m_project == nullptr)
        {
            if (error != nullptr)
            {
                *error = "No project is currently open.";
            }
            return false;
        }

        const std::filesystem::path scenePath = NormalizeScenePath(state, rawScenePath, !requireExisting);
        if (scenePath.empty() || !scenePath.has_filename())
        {
            if (error != nullptr)
            {
                *error = "The selected scene path is empty.";
            }
            return false;
        }

        if (scenePath.extension() != ".hscene")
        {
            if (error != nullptr)
            {
                *error = "Scene files must use the .hscene extension.";
            }
            return false;
        }

        const std::filesystem::path assetsDirectory = GetProjectAssetsDirectory(state);
        if (!IsPathWithinDirectory(scenePath, assetsDirectory))
        {
            if (error != nullptr)
            {
                *error = "Scene files must be stored inside the project's assets directory.";
            }
            return false;
        }

        if (requireExisting)
        {
            std::error_code ec;
            if (!std::filesystem::exists(scenePath, ec) || ec)
            {
                if (error != nullptr)
                {
                    *error = "The selected scene file does not exist.";
                }
                return false;
            }

            if (!std::filesystem::is_regular_file(scenePath, ec) || ec)
            {
                if (error != nullptr)
                {
                    *error = "The selected scene path is not a regular file.";
                }
                return false;
            }
        }

        if (resolvedScenePath != nullptr)
        {
            *resolvedScenePath = scenePath;
        }
        return true;
    }

    std::filesystem::path GetPreferredScenePath(LauncherState& state)
    {
        if (!state.m_currentScenePath.IsEmpty())
        {
            return std::filesystem::path{state.m_currentScenePath.CStr()};
        }

        const wax::StringView startupScene = state.m_project->Project().StartupSceneRelative();
        if (!startupScene.IsEmpty())
        {
            return ResolveScenePath(*state.m_project, startupScene);
        }

        return GetProjectAssetsDirectory(state) / "scenes" / "main.hscene";
    }

    bool PickScenePath(LauncherState& state, NativePathPickerMode mode, std::filesystem::path* selectedPath)
    {
        std::filesystem::path seedPath = GetPreferredScenePath(state);
        if (seedPath.empty())
        {
            seedPath = GetProjectAssetsDirectory(state) / "scenes" / "main.hscene";
        }

        if (mode == NativePathPickerMode::SCENE_FILE_OPEN)
        {
            std::error_code ec;
            if (!std::filesystem::exists(seedPath, ec) || ec)
            {
                seedPath = seedPath.parent_path();
            }
        }

        const NativePathPickerResult result = ShowNativePathPicker(mode, seedPath, selectedPath);
        if (result == NativePathPickerResult::SUCCESS)
        {
            return true;
        }

        if (result == NativePathPickerResult::FAILED)
        {
            hive::LogWarning(LOG_LAUNCHER, "The native scene file picker failed to open.");
        }
        else if (result == NativePathPickerResult::UNAVAILABLE)
        {
            hive::LogWarning(LOG_LAUNCHER, "The native scene file picker is not available on this platform.");
        }

        return false;
    }

    bool LoadEditorSceneAsset(waggle::EngineContext& ctx, LauncherState& state,
                              const std::filesystem::path& rawScenePath)
    {
        std::filesystem::path scenePath{};
        wax::String error{};
        if (!ResolveSceneAssetPath(state, rawScenePath, true, &scenePath, &error))
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to open scene '{}': {}", rawScenePath.generic_string(),
                             error.CStr());
            return false;
        }

        if (!LoadEditorScene(ctx, state, scenePath))
        {
            return false;
        }

        hive::LogInfo(LOG_LAUNCHER, "Scene opened: {}", scenePath.generic_string());
        return true;
    }

    bool SaveEditorSceneToPath(waggle::EngineContext& ctx, LauncherState& state,
                               const std::filesystem::path& rawScenePath)
    {
        std::filesystem::path scenePath{};
        wax::String error{};
        if (!ResolveSceneAssetPath(state, rawScenePath, false, &scenePath, &error))
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to save scene '{}': {}", rawScenePath.generic_string(),
                             error.CStr());
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(scenePath.parent_path(), ec);
        if (ec)
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to create scene directory: {}",
                             scenePath.parent_path().generic_string());
            return false;
        }

        const wax::String scenePathGeneric{scenePath.generic_string().c_str()};
        if (!forge::SaveScene(*ctx.m_world, state.m_componentRegistry, scenePathGeneric.CStr()))
        {
            return false;
        }

        state.m_currentScenePath = scenePathGeneric;
        state.m_currentSceneRelative = MakeSceneRelativePath(*state.m_project, scenePath);
        state.m_sceneAutosaveElapsedSeconds = 0.0;
        state.m_sceneDirty = false;
        ClearSceneRecoveryArtifacts(state);
        hive::LogInfo(LOG_LAUNCHER, "Scene saved: {}", scenePathGeneric.CStr());
        return true;
    }

    bool SaveSceneRecoverySnapshot(waggle::EngineContext& ctx, LauncherState& state)
    {
        if (state.m_project == nullptr || !state.m_sceneDirty)
        {
            return false;
        }

        const wax::String sceneRelative = GetSceneRecoverySourceRelative(state);
        if (sceneRelative.IsEmpty())
        {
            return false;
        }

        const std::filesystem::path recoveryDirectory = GetSceneRecoveryDirectory(state);
        std::error_code ec;
        std::filesystem::create_directories(recoveryDirectory, ec);
        if (ec)
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to create recovery directory: {}",
                             recoveryDirectory.generic_string());
            return false;
        }

        const std::filesystem::path recoveryScenePath = GetSceneRecoveryScenePath(state);
        const wax::String recoveryScenePathString{recoveryScenePath.generic_string().c_str()};
        if (!forge::SaveScene(*ctx.m_world, state.m_componentRegistry, recoveryScenePathString.CStr()))
        {
            return false;
        }

        if (!WriteSceneRecoveryMetadata(state, sceneRelative))
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to write recovery metadata: {}",
                             GetSceneRecoveryMetadataPath(state).generic_string());
            return false;
        }

        hive::LogInfo(LOG_LAUNCHER, "Scene autosaved for recovery: {}", sceneRelative.CStr());
        return true;
    }

    wax::String CaptureSceneBackup(queen::World& world, const queen::ComponentRegistry<256>& registry)
    {
        queen::DynamicWorldSerializer serializer{};
        const auto result = serializer.Serialize(world, registry);
        if (!result.m_success)
        {
            return {};
        }

        return serializer.TakeString();
    }

    bool RestoreSceneBackup(queen::World& world, const queen::ComponentRegistry<256>& registry,
                            const wax::String& backupScene)
    {
        if (backupScene.IsEmpty())
        {
            return false;
        }

        const auto result = queen::WorldDeserializer::Deserialize(world, registry, backupScene.CStr());
        return result.m_success;
    }

    bool LoadEditorScene(waggle::EngineContext& ctx, LauncherState& state, const std::filesystem::path& scenePath)
    {
        if (scenePath.empty())
        {
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(scenePath, ec) || ec)
        {
            hive::LogInfo(LOG_LAUNCHER, "Scene file not found: {}", scenePath.generic_string());
            return false;
        }

        // The first startup-scene load happens before the editor owns a scene. Taking a
        // serialized backup there is unnecessary and can walk non-scene bootstrap state.
        const bool hasPreviousScene = !state.m_currentScenePath.IsEmpty() || !state.m_currentSceneRelative.IsEmpty();
        const wax::String previousSceneBackup =
            hasPreviousScene ? CaptureSceneBackup(*ctx.m_world, state.m_componentRegistry) : wax::String{};
        const wax::String previousScenePath = state.m_currentScenePath;
        const wax::String previousSceneRelative = state.m_currentSceneRelative;
        const bool previousSceneDirty = state.m_sceneDirty;

        ClearWorldEntities(*ctx.m_world);
        ResetSceneEditorState(state);

        const wax::String scenePathString{scenePath.generic_string().c_str()};
        if (!forge::LoadScene(*ctx.m_world, state.m_componentRegistry, scenePathString.CStr()))
        {
            if (hasPreviousScene && RestoreSceneBackup(*ctx.m_world, state.m_componentRegistry, previousSceneBackup))
            {
                state.m_currentScenePath = previousScenePath;
                state.m_currentSceneRelative = previousSceneRelative;
                state.m_sceneDirty = previousSceneDirty;
                hive::LogWarning(LOG_LAUNCHER, "Scene load failed; restored the previous scene state.");
            }
            else if (hasPreviousScene && !previousSceneBackup.IsEmpty())
            {
                hive::LogWarning(LOG_LAUNCHER,
                                 "Scene load failed and the previous scene backup could not be restored.");
            }
            hive::LogWarning(LOG_LAUNCHER, "Failed to load scene: {}", scenePathString.CStr());
            return false;
        }

        state.m_currentScenePath = scenePathString;
        state.m_currentSceneRelative = MakeSceneRelativePath(*state.m_project, scenePath);
        state.m_sceneAutosaveElapsedSeconds = 0.0;
        state.m_sceneDirty = false;
        return true;
    }

    [[maybe_unused]] bool RestoreSceneRecovery(waggle::EngineContext& ctx, LauncherState& state)
    {
        const wax::String recoveredSceneRelative = state.m_recoverySceneRelative;
        if (recoveredSceneRelative.IsEmpty())
        {
            return false;
        }

        const std::filesystem::path recoveryScenePath = GetSceneRecoveryScenePath(state);
        if (!LoadEditorScene(ctx, state, recoveryScenePath))
        {
            return false;
        }

        const std::filesystem::path restoredScenePath =
            NormalizeScenePath(state, std::filesystem::path{recoveredSceneRelative.CStr()}, false);
        state.m_currentScenePath = wax::String{restoredScenePath.generic_string().c_str()};
        state.m_currentSceneRelative = recoveredSceneRelative;
        state.m_sceneAutosaveElapsedSeconds = 0.0;
        state.m_sceneDirty = true;
        ClearSceneRecoveryArtifacts(state);
        hive::LogInfo(LOG_LAUNCHER, "Recovered autosaved scene state for {}", restoredScenePath.generic_string());
        return true;
    }

    [[maybe_unused]] void UpdateSceneAutosave(waggle::EngineContext& ctx, LauncherState& state)
    {
        constexpr double kAutosaveIntervalSeconds = 20.0;

        if (state.m_project == nullptr || !state.m_sceneDirty)
        {
            state.m_sceneAutosaveElapsedSeconds = 0.0;
            return;
        }

        if (state.m_playState != forge::PlayState::EDITING)
        {
            return;
        }

        const auto* frameInfo = ctx.m_world->Resource<waggle::FrameInfo>();
        if (frameInfo == nullptr || frameInfo->m_realDt <= 0.0f)
        {
            return;
        }

        state.m_sceneAutosaveElapsedSeconds += static_cast<double>(frameInfo->m_realDt);
        if (state.m_sceneAutosaveElapsedSeconds < kAutosaveIntervalSeconds)
        {
            return;
        }

        state.m_sceneAutosaveElapsedSeconds = 0.0;
        (void)SaveSceneRecoverySnapshot(ctx, state);
    }

    bool SaveEditorScene(waggle::EngineContext& ctx, LauncherState& state)
    {
        if (state.m_project == nullptr)
        {
            return false;
        }

        const std::filesystem::path scenePath = GetPreferredScenePath(state);
        if (scenePath.empty())
        {
            return false;
        }

        return SaveEditorSceneToPath(ctx, state, scenePath);
    }

    bool SaveEditorSceneAs(waggle::EngineContext& ctx, LauncherState& state)
    {
        std::filesystem::path selectedPath{};
        if (!PickScenePath(state, NativePathPickerMode::SCENE_FILE_SAVE, &selectedPath))
        {
            return false;
        }

        return SaveEditorSceneToPath(ctx, state, selectedPath);
    }

    bool ReloadEditorScene(waggle::EngineContext& ctx, LauncherState& state)
    {
        const std::filesystem::path scenePath = GetPreferredScenePath(state);
        if (scenePath.empty())
        {
            return false;
        }

        return LoadEditorScene(ctx, state, scenePath);
    }

    void NewEditorScene(waggle::EngineContext& ctx, LauncherState& state)
    {
        ClearWorldEntities(*ctx.m_world);
        ResetSceneEditorState(state);
        const std::filesystem::path scenePath = GetPreferredScenePath(state);
        state.m_currentScenePath = wax::String{scenePath.generic_string().c_str()};
        state.m_currentSceneRelative = MakeSceneRelativePath(*state.m_project, scenePath);
        state.m_sceneDirty = true;
        hive::LogInfo(LOG_LAUNCHER, "New scene initialized at {}", state.m_currentScenePath.CStr());
    }

    [[maybe_unused]] const char* DescribePendingEditorAction(PendingEditorAction action) noexcept
    {
        switch (action)
        {
            case PendingEditorAction::NEW_SCENE:
                return "create a new scene";
            case PendingEditorAction::OPEN_SCENE:
                return "open another scene";
            case PendingEditorAction::RELOAD_SCENE:
                return "reload the current scene";
            case PendingEditorAction::EXIT_APP:
                return "exit the editor";
            case PendingEditorAction::NONE:
                break;
        }

        return "continue";
    }

    void ExecutePendingEditorAction(waggle::EngineContext& ctx, LauncherState& state)
    {
        const PendingEditorAction action = state.m_pendingEditorAction;
        const std::filesystem::path pendingScenePath{state.m_pendingScenePath.CStr()};
        state.m_pendingEditorAction = PendingEditorAction::NONE;
        state.m_pendingScenePath.Clear();

        switch (action)
        {
            case PendingEditorAction::NEW_SCENE:
                NewEditorScene(ctx, state);
                break;
            case PendingEditorAction::OPEN_SCENE:
                (void)LoadEditorSceneAsset(ctx, state, pendingScenePath);
                break;
            case PendingEditorAction::RELOAD_SCENE:
                (void)ReloadEditorScene(ctx, state);
                break;
            case PendingEditorAction::EXIT_APP:
                ctx.m_app->RequestStop();
                break;
            case PendingEditorAction::NONE:
                break;
        }
    }

    void RequestPendingEditorAction(waggle::EngineContext& ctx, LauncherState& state, PendingEditorAction action,
                                    const std::filesystem::path& pendingScenePath)
    {
        state.m_pendingScenePath = wax::String{pendingScenePath.generic_string().c_str()};

        if (!state.m_sceneDirty)
        {
            state.m_pendingEditorAction = action;
            ExecutePendingEditorAction(ctx, state);
            return;
        }

        state.m_pendingEditorAction = action;
        state.m_scenePromptError.Clear();
        state.m_openUnsavedScenePopup = true;
    }

    bool OpenEditorSceneWithPicker(waggle::EngineContext& ctx, LauncherState& state)
    {
        std::filesystem::path selectedPath{};
        if (!PickScenePath(state, NativePathPickerMode::SCENE_FILE_OPEN, &selectedPath))
        {
            return false;
        }

        RequestPendingEditorAction(ctx, state, PendingEditorAction::OPEN_SCENE, selectedPath);
        return true;
    }

} // namespace brood::launcher

#endif
