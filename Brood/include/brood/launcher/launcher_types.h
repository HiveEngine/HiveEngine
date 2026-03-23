#pragma once

#include <hive/hive_config.h>
#include <hive/core/log.h>

#include <comb/default_allocator.h>

#include <waggle/project/gameplay_module.h>
#include <waggle/project/project_manager.h>

#if HIVE_MODE_EDITOR
#include <queen/reflect/component_registry.h>

#include <forge/forge_main_window.h>
#include <forge/selection.h>
#include <forge/toolbar.h>
#include <forge/undo.h>
#endif

#include <wax/containers/array.h>
#include <wax/containers/string.h>
#include <wax/containers/vector.h>

#include <cstdint>
#include <filesystem>
#include <memory>

class QProcess;

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
        wax::String m_name;
        wax::String m_version;
        wax::String m_path;
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
        wax::Array<char, kPathBufferSize> m_openPath{};
        wax::Array<char, kPathBufferSize> m_createDirectory{};
        wax::Array<char, kNameBufferSize> m_createName{};
        wax::Array<char, kVersionBufferSize> m_createVersion{};
        std::filesystem::path m_engineRoot{};
        wax::Vector<LauncherDiscoveredProject> m_discoveredProjects;
        wax::String m_statusMessage;
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
        wax::String m_projectPath;
        wax::String m_windowTitle{"Forge Editor"};
        uint32_t m_windowWidth{1920};
        uint32_t m_windowHeight{1080};
        bool m_exitAfterSetup{false};
        bool m_projectOpen{false};
        bool m_pendingDllReload{false};

#if HIVE_MODE_EDITOR
        ProjectHubState m_hub;
        forge::EditorSelection m_selection;
        std::unique_ptr<forge::UndoStack> m_undo{std::make_unique<forge::UndoStack>()};
        forge::GizmoState m_gizmo;
        forge::PlayState m_playState{forge::PlayState::EDITING};
        queen::ComponentRegistry<256> m_componentRegistry;
        wax::String m_assetsRoot;
        wax::String m_currentSceneRelative;
        wax::String m_currentScenePath;
        wax::String m_scenePromptError;
        PendingEditorAction m_pendingEditorAction{PendingEditorAction::NONE};
        wax::String m_pendingScenePath;
        wax::String m_recoverySceneRelative;
        wax::String m_recoveryPromptError;
        double m_sceneAutosaveElapsedSeconds{0.0};
        bool m_sceneDirty{false};
        bool m_openUnsavedScenePopup{false};
        bool m_openRecoveryScenePopup{false};
        bool m_firstFrame{true};
        forge::ForgeMainWindow* m_mainWindow{nullptr};
        QProcess* m_buildProcess{nullptr};
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
