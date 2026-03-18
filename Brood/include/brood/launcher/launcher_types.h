#pragma once

#include <hive/HiveConfig.h>
#include <hive/core/log.h>

#include <comb/default_allocator.h>

#include <waggle/project/gameplay_module.h>
#include <waggle/project/project_manager.h>

#if HIVE_MODE_EDITOR
#include <queen/reflect/component_registry.h>

#include <forge/forge_main_window.h>
#include <forge/gizmo.h>
#include <forge/selection.h>
#include <forge/toolbar.h>
#include <forge/undo.h>
#endif

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace brood::launcher
{

    inline const hive::LogCategory LOG_LAUNCHER{"Hive.Launcher"};

#if HIVE_MODE_EDITOR
    constexpr size_t kPathBufferSize = 1024;
    constexpr size_t kNameBufferSize = 128;
    constexpr size_t kVersionBufferSize = 32;

    enum class LauncherHubPage
    {
        HUB_OPEN_EXISTING,
        HUB_CREATE_NEW,
    };

    enum class NativePathPickerMode
    {
        PROJECT_FILE,
        DIRECTORY,
        SCENE_FILE_OPEN,
        SCENE_FILE_SAVE,
    };

    enum class NativePathPickerResult
    {
        SUCCESS,
        CANCELLED,
        UNAVAILABLE,
        FAILED,
    };

#if HIVE_PLATFORM_WINDOWS
    enum class LauncherToolchainPreset
    {
        LLVM,
        MSVC,
    };
#else
    enum class LauncherToolchainPreset
    {
        LLVM,
        GCC,
    };
#endif

    struct LauncherDiscoveredProject
    {
        std::string m_name;
        std::string m_version;
        std::string m_path;
        bool m_isCurrentDirectory{false};
    };

    struct ProjectHubState
    {
        LauncherHubPage m_page{LauncherHubPage::HUB_OPEN_EXISTING};
        LauncherToolchainPreset m_toolchain{
#if HIVE_PLATFORM_WINDOWS
            LauncherToolchainPreset::LLVM
#else
            LauncherToolchainPreset::LLVM
#endif
        };
        std::array<char, kPathBufferSize> m_openPath{};
        std::array<char, kPathBufferSize> m_createDirectory{};
        std::array<char, kNameBufferSize> m_createName{};
        std::array<char, kVersionBufferSize> m_createVersion{};
        std::filesystem::path m_engineRoot{};
        std::vector<LauncherDiscoveredProject> m_discoveredProjects;
        std::string m_statusMessage;
        bool m_statusIsError{false};
        bool m_supportEditor{true};
        bool m_supportGame{true};
        bool m_supportHeadless{true};
    };

    enum class PendingEditorAction
    {
        NONE,
        NEW_SCENE,
        OPEN_SCENE,
        RELOAD_SCENE,
        EXIT_APP,
    };
#endif

    struct LauncherState
    {
        comb::ModuleAllocator m_alloc{"Launcher", size_t{1024} * 1024 * 1024};
        waggle::ProjectManager* m_project{nullptr};
        waggle::GameplayModule m_gameplay;
        std::string m_projectPath;
        std::string m_windowTitle{"Forge Editor"};
        uint32_t m_windowWidth{1920};
        uint32_t m_windowHeight{1080};
        bool m_exitAfterSetup{false};
        bool m_projectOpen{false};

#if HIVE_MODE_EDITOR
        ProjectHubState m_hub;
        forge::EditorSelection m_selection;
        std::unique_ptr<forge::UndoStack> m_undo{std::make_unique<forge::UndoStack>()};
        forge::GizmoState m_gizmo;
        forge::PlayState m_playState{forge::PlayState::EDITING};
        queen::ComponentRegistry<256> m_componentRegistry;
        std::string m_assetsRoot;
        std::string m_currentSceneRelative;
        std::string m_currentScenePath;
        std::string m_scenePromptError;
        PendingEditorAction m_pendingEditorAction{PendingEditorAction::NONE};
        std::string m_pendingScenePath;
        std::string m_recoverySceneRelative;
        std::string m_recoveryPromptError;
        double m_sceneAutosaveElapsedSeconds{0.0};
        bool m_sceneDirty{false};
        bool m_openUnsavedScenePopup{false};
        bool m_openRecoveryScenePopup{false};
        bool m_firstFrame{true};
        forge::ForgeMainWindow* m_mainWindow{nullptr};
#endif
    };

    enum class LauncherRequestedMode
    {
        AUTO,
        EDITOR,
        GAME,
        HEADLESS,
    };

    struct LauncherCommandLine
    {
        std::filesystem::path m_projectPath{};
        LauncherRequestedMode m_requestedMode{LauncherRequestedMode::AUTO};
        bool m_exitAfterSetup{false};
    };

} // namespace brood::launcher
