#include <hive/HiveConfig.h>
#include <hive/core/log.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <nectar/project/project_file.h>

#include <waggle/engine_runner.h>
#include <waggle/app_context.h>
#include <waggle/components/camera.h>
#include <waggle/components/lighting.h>
#include <waggle/components/transform.h>
#include <waggle/project/gameplay_module.h>
#include <waggle/project/project_context.h>
#include <waggle/project/project_manager.h>
#include <waggle/project/project_scaffolder.h>
#include <waggle/time.h>

#include <terra/terra.h>

#include <swarm/swarm.h>

#if HIVE_MODE_EDITOR
#include <queen/reflect/world_deserializer.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/world_serializer.h>

#include <terra/platform/glfw_terra.h>

#include <forge/forge_main_window.h>
#include <forge/gizmo.h>
#include <forge/project_hub.h>
#include <forge/scene_file.h>
#include <forge/selection.h>
#include <forge/theme.h>
#include <forge/toolbar.h>
#include <forge/undo.h>

#include <QApplication>
#endif

#if HIVE_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shobjidl.h>
#endif

#include <algorithm>
#include <array>
#include <brood/process_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{

    static const hive::LogCategory LOG_LAUNCHER{"Hive.Launcher"};

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

#if HIVE_MODE_EDITOR
#endif

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

    const char* GetLauncherModeArgumentName(LauncherRequestedMode mode)
    {
        switch (mode)
        {
            case LauncherRequestedMode::EDITOR:
                return "--editor";
            case LauncherRequestedMode::GAME:
                return "--game";
            case LauncherRequestedMode::HEADLESS:
                return "--headless";
            case LauncherRequestedMode::AUTO:
            default:
                return "auto";
        }
    }

    const char* GetCompiledLauncherModeName()
    {
#if HIVE_MODE_EDITOR
        return "editor";
#elif HIVE_MODE_HEADLESS
        return "headless";
#else
        return "game";
#endif
    }

    LauncherRequestedMode GetDefaultLauncherMode()
    {
#if HIVE_MODE_EDITOR
        return LauncherRequestedMode::EDITOR;
#elif HIVE_MODE_HEADLESS
        return LauncherRequestedMode::HEADLESS;
#else
        return LauncherRequestedMode::GAME;
#endif
    }

    bool IsLauncherModeSupported(LauncherRequestedMode mode)
    {
        switch (mode)
        {
            case LauncherRequestedMode::AUTO:
                return true;
#if HIVE_MODE_EDITOR
            case LauncherRequestedMode::EDITOR:
            case LauncherRequestedMode::HEADLESS:
                return true;
            case LauncherRequestedMode::GAME:
                return false;
#elif HIVE_MODE_HEADLESS
            case LauncherRequestedMode::HEADLESS:
                return true;
            case LauncherRequestedMode::EDITOR:
            case LauncherRequestedMode::GAME:
                return false;
#else
            case LauncherRequestedMode::GAME:
            case LauncherRequestedMode::HEADLESS:
                return true;
            case LauncherRequestedMode::EDITOR:
                return false;
#endif
        }

        return false;
    }

    bool TryAssignRequestedMode(LauncherRequestedMode mode, LauncherCommandLine* commandLine)
    {
        if (commandLine->m_requestedMode == LauncherRequestedMode::AUTO || commandLine->m_requestedMode == mode)
        {
            commandLine->m_requestedMode = mode;
            return true;
        }

        std::fprintf(stderr, "Error: conflicting launcher modes: %s and %s\n",
                     GetLauncherModeArgumentName(commandLine->m_requestedMode), GetLauncherModeArgumentName(mode));
        return false;
    }

    bool ResolveLauncherMode(const LauncherCommandLine& commandLine, waggle::EngineMode* outMode)
    {
        LauncherRequestedMode mode = commandLine.m_requestedMode;
        if (mode == LauncherRequestedMode::AUTO)
        {
            mode = GetDefaultLauncherMode();
        }

        if (!IsLauncherModeSupported(mode))
        {
            std::fprintf(stderr,
                         "Error: this launcher binary was built in %s mode and does not support %s\n",
                         GetCompiledLauncherModeName(), GetLauncherModeArgumentName(mode));
            return false;
        }

        switch (mode)
        {
            case LauncherRequestedMode::EDITOR:
                *outMode = waggle::EngineMode::EDITOR;
                return true;
            case LauncherRequestedMode::GAME:
                *outMode = waggle::EngineMode::GAME;
                return true;
            case LauncherRequestedMode::HEADLESS:
                *outMode = waggle::EngineMode::HEADLESS;
                return true;
            case LauncherRequestedMode::AUTO:
            default:
                break;
        }

        return false;
    }

    void PrintUsage()
    {
        std::fprintf(stderr,
                     "Usage: hive_launcher [--editor|--game|--headless] [--project path/to/project.hive]\n"
                     "                     [--exit-after-setup] [path/to/project.hive]\n"
                     "       Positional project paths remain supported for compatibility.\n");
    }

    bool TryParseCommandLine(int argc, char* argv[], LauncherCommandLine* commandLine)
    {
        for (int i = 1; i < argc; ++i)
        {
            const char* arg = argv[i];
            if (std::strcmp(arg, "--editor") == 0)
            {
                if (!TryAssignRequestedMode(LauncherRequestedMode::EDITOR, commandLine))
                {
                    PrintUsage();
                    return false;
                }
                continue;
            }

            if (std::strcmp(arg, "--game") == 0)
            {
                if (!TryAssignRequestedMode(LauncherRequestedMode::GAME, commandLine))
                {
                    PrintUsage();
                    return false;
                }
                continue;
            }

            if (std::strcmp(arg, "--headless") == 0)
            {
                if (!TryAssignRequestedMode(LauncherRequestedMode::HEADLESS, commandLine))
                {
                    PrintUsage();
                    return false;
                }
                continue;
            }

            if (std::strcmp(arg, "--exit-after-setup") == 0)
            {
                commandLine->m_exitAfterSetup = true;
                continue;
            }

            if (std::strcmp(arg, "--project") == 0)
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "Error: --project requires a file path\n");
                    PrintUsage();
                    return false;
                }

                if (!commandLine->m_projectPath.empty())
                {
                    std::fprintf(stderr, "Error: multiple project paths provided\n");
                    PrintUsage();
                    return false;
                }

                commandLine->m_projectPath = argv[++i];
                continue;
            }

            if (arg[0] == '-')
            {
                std::fprintf(stderr, "Error: unknown option: %s\n", arg);
                PrintUsage();
                return false;
            }

            if (!commandLine->m_projectPath.empty())
            {
                std::fprintf(stderr, "Error: multiple project paths provided\n");
                PrintUsage();
                return false;
            }

            commandLine->m_projectPath = arg;
        }

        return true;
    }

#if HIVE_MODE_EDITOR
    std::filesystem::path GetCurrentExecutablePath()
    {
#if HIVE_PLATFORM_WINDOWS
        std::wstring buffer(static_cast<size_t>(MAX_PATH), L'\0');

        while (true)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return {};
            }

            if (length < buffer.size() - 1)
            {
                buffer.resize(length);
                return std::filesystem::path{buffer};
            }

            buffer.resize(buffer.size() * 2);
        }
#else
        std::error_code ec;
        const std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe", ec);
        return ec ? std::filesystem::path{} : exePath;
#endif
    }

    bool IsEngineRoot(const std::filesystem::path& path)
    {
        return std::filesystem::exists(path / "CMakeLists.txt") && std::filesystem::exists(path / "HiveFeatures.json");
    }

    std::string GetEnvironmentValue(const char* name)
    {
#if HIVE_PLATFORM_WINDOWS
        char* value = nullptr;
        size_t valueLength = 0;
        if (_dupenv_s(&value, &valueLength, name) != 0 || value == nullptr)
        {
            return {};
        }

        std::string result{value};
        std::free(value);
        return result;
#else
        const char* value = std::getenv(name);
        return value != nullptr ? std::string{value} : std::string{};
#endif
    }

    std::filesystem::path FindEngineRoot()
    {
        const std::string enginePath = GetEnvironmentValue("HIVE_ENGINE_DIR");
        if (!enginePath.empty())
        {
            const std::filesystem::path root{enginePath};
            if (IsEngineRoot(root))
            {
                return root;
            }
        }

        std::filesystem::path current = GetCurrentExecutablePath().parent_path();
        for (int i = 0; i < 10 && !current.empty(); ++i)
        {
            if (IsEngineRoot(current))
            {
                return current;
            }

            const std::filesystem::path parent = current.parent_path();
            if (parent == current)
            {
                break;
            }
            current = parent;
        }

        return {};
    }

    std::filesystem::path GetHomeDirectory()
    {
#if HIVE_PLATFORM_WINDOWS
        const std::string userProfile = GetEnvironmentValue("USERPROFILE");
        if (!userProfile.empty())
        {
            return std::filesystem::path{userProfile};
        }

        const std::string homeDrive = GetEnvironmentValue("HOMEDRIVE");
        const std::string homePath = GetEnvironmentValue("HOMEPATH");
        if (!homeDrive.empty() && !homePath.empty())
        {
            return std::filesystem::path{homeDrive + homePath};
        }
#else
        const std::string home = GetEnvironmentValue("HOME");
        if (!home.empty())
        {
            return std::filesystem::path{home};
        }
#endif

        std::error_code ec;
        const std::filesystem::path current = std::filesystem::current_path(ec);
        return ec ? std::filesystem::path{} : current;
    }

    std::filesystem::path GetDefaultProjectsDirectory()
    {
        const std::filesystem::path home = GetHomeDirectory();
        if (home.empty())
        {
            return {};
        }

#if HIVE_PLATFORM_WINDOWS
        return home / "Documents" / "HiveProjects";
#else
        return home / "HiveProjects";
#endif
    }

    void CopyStringToBuffer(const std::string& value, char* buffer, size_t bufferSize)
    {
        if (bufferSize == 0)
        {
            return;
        }

        const size_t copyLength = (std::min)(bufferSize - 1, value.size());
        if (copyLength > 0)
        {
            std::memcpy(buffer, value.data(), copyLength);
        }
        buffer[copyLength] = '\0';
    }

    std::string TrimmedCopy(const char* input)
    {
        if (input == nullptr)
        {
            return {};
        }

        std::string value{input};
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            return {};
        }

        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    bool IsProjectNameValid(const std::string& name)
    {
        if (name.empty())
        {
            return false;
        }

        for (const char ch : name)
        {
            const bool isAlphaNum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
            if (!isAlphaNum && ch != '_' && ch != '-')
            {
                return false;
            }
        }

        return true;
    }
#endif

    bool ResolveProjectFilePath(const std::filesystem::path& inputPath, std::filesystem::path* resolvedPath)
    {
        if (inputPath.empty())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::path candidate = inputPath;
        if (std::filesystem::is_directory(candidate, ec) && !ec)
        {
            candidate /= "project.hive";
        }

        if (!std::filesystem::exists(candidate, ec) || ec)
        {
            return false;
        }

        candidate = std::filesystem::absolute(candidate, ec);
        if (ec)
        {
            return false;
        }

        *resolvedPath = candidate;
        return true;
    }

#if HIVE_MODE_EDITOR
    bool TryLoadProjectSummary(const std::filesystem::path& path, std::string* name, std::string* version)
    {
        comb::ModuleAllocator tmpAlloc{"TmpProjectSummary", size_t{4} * 1024 * 1024};
        nectar::ProjectFile projectFile{tmpAlloc.Get()};
        const std::string projectPath = path.generic_string();
        auto loadResult = projectFile.LoadFromDisk({projectPath.c_str(), projectPath.size()});
        if (!loadResult.m_success)
        {
            return false;
        }

        name->assign(projectFile.Name().Data(), projectFile.Name().Size());
        version->assign(projectFile.Version().Data(), projectFile.Version().Size());
        return true;
    }
#endif

    std::string BuildWindowTitle(const std::string& projectPath)
    {
        std::string windowTitle = "HiveEngine";
        comb::ModuleAllocator tmpAlloc{"TmpProjectParse", size_t{4} * 1024 * 1024};
        nectar::ProjectFile projectFile{tmpAlloc.Get()};
        auto loadResult = projectFile.LoadFromDisk({projectPath.c_str(), projectPath.size()});
        if (loadResult.m_success && projectFile.Name().Size() > 0)
        {
            windowTitle += " - ";
            windowTitle.append(projectFile.Name().Data(), projectFile.Name().Size());
        }
        else
        {
            windowTitle += " - ";
            windowTitle += std::filesystem::path{projectPath}.parent_path().filename().string();
        }

        return windowTitle;
    }

#if HIVE_MODE_EDITOR
    const char* GetPresetBase(LauncherToolchainPreset preset)
    {
#if HIVE_PLATFORM_WINDOWS
        return preset == LauncherToolchainPreset::MSVC ? "msvc-windows-base" : "llvm-windows-base";
#else
        return preset == LauncherToolchainPreset::GCC ? "gcc-linux-base" : "llvm-linux-base";
#endif
    }

    std::vector<LauncherDiscoveredProject> DiscoverProjects(const std::filesystem::path& engineRoot)
    {
        std::vector<LauncherDiscoveredProject> projects;

        std::error_code ec;
        const std::filesystem::path currentProject = std::filesystem::current_path(ec) / "project.hive";
        if (!ec && std::filesystem::exists(currentProject))
        {
            LauncherDiscoveredProject summary{};
            summary.m_path = currentProject.generic_string();
            summary.m_isCurrentDirectory = true;
            if (!TryLoadProjectSummary(currentProject, &summary.m_name, &summary.m_version))
            {
                summary.m_name = currentProject.parent_path().filename().string();
                summary.m_version = "Unknown version";
            }
            projects.push_back(summary);
        }

        if (!engineRoot.empty())
        {
            const std::filesystem::path projectsRoot = engineRoot / "projects";
            std::filesystem::directory_iterator end{};
            for (std::filesystem::directory_iterator it{projectsRoot, ec}; !ec && it != end; it.increment(ec))
            {
                const std::filesystem::path projectFile = it->path() / "project.hive";
                if (!std::filesystem::exists(projectFile))
                {
                    continue;
                }

                LauncherDiscoveredProject summary{};
                summary.m_path = projectFile.generic_string();
                if (!TryLoadProjectSummary(projectFile, &summary.m_name, &summary.m_version))
                {
                    summary.m_name = it->path().filename().string();
                    summary.m_version = "Unknown version";
                }
                projects.push_back(summary);
            }
        }

        std::sort(projects.begin(), projects.end(), [](const LauncherDiscoveredProject& lhs,
                                                       const LauncherDiscoveredProject& rhs) {
            if (lhs.m_isCurrentDirectory != rhs.m_isCurrentDirectory)
            {
                return lhs.m_isCurrentDirectory;
            }
            return lhs.m_name < rhs.m_name;
        });

        return projects;
    }

    void SetHubStatus(ProjectHubState& hub, bool isError, const std::string& message)
    {
        hub.m_statusIsError = isError;
        hub.m_statusMessage = message;
    }

    void RefreshProjectHub(LauncherState& state)
    {
        state.m_hub.m_discoveredProjects = DiscoverProjects(state.m_hub.m_engineRoot);
    }

    void InitializeProjectHub(LauncherState& state)
    {
        state.m_hub.m_engineRoot = FindEngineRoot();
        state.m_hub.m_discoveredProjects = DiscoverProjects(state.m_hub.m_engineRoot);

        const std::filesystem::path defaultDirectory = GetDefaultProjectsDirectory();
        if (!defaultDirectory.empty())
        {
            CopyStringToBuffer(defaultDirectory.generic_string(), state.m_hub.m_createDirectory.data(),
                               state.m_hub.m_createDirectory.size());
        }

        CopyStringToBuffer("MyGame", state.m_hub.m_createName.data(), state.m_hub.m_createName.size());
        CopyStringToBuffer("0.1.0", state.m_hub.m_createVersion.data(), state.m_hub.m_createVersion.size());

        std::error_code ec;
        const std::filesystem::path currentProject = std::filesystem::current_path(ec) / "project.hive";
        if (!ec && std::filesystem::exists(currentProject))
        {
            CopyStringToBuffer(currentProject.generic_string(), state.m_hub.m_openPath.data(),
                               state.m_hub.m_openPath.size());
        }
    }

    std::filesystem::path BuildCreateTargetPath(const ProjectHubState& hub, const std::string& projectName)
    {
        const std::string createDirectory = TrimmedCopy(hub.m_createDirectory.data());
        if (createDirectory.empty() || projectName.empty())
        {
            return {};
        }

        return std::filesystem::path{createDirectory} / projectName;
    }

    [[maybe_unused]] std::filesystem::path GetPickerSeedDirectory(const char* rawPath)
    {
        const std::string value = TrimmedCopy(rawPath);
        if (value.empty())
        {
            return {};
        }

        std::error_code ec;
        const std::filesystem::path candidate{value};
        if (std::filesystem::exists(candidate, ec) && !ec)
        {
            if (std::filesystem::is_directory(candidate, ec) && !ec)
            {
                return candidate;
            }
            return candidate.parent_path();
        }

        return candidate.parent_path();
    }

#if HIVE_PLATFORM_WINDOWS
    class ScopedComApartment
    {
    public:
        ScopedComApartment() : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))
        {
        }

        ~ScopedComApartment()
        {
            if (SUCCEEDED(m_result))
            {
                CoUninitialize();
            }
        }

        bool IsReady() const
        {
            return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
        }

    private:
        HRESULT m_result;
    };

    template <typename T>
    class ScopedComPtr
    {
    public:
        ScopedComPtr() = default;

        ~ScopedComPtr()
        {
            Reset();
        }

        T* Get() const
        {
            return m_ptr;
        }

        T** Put()
        {
            Reset();
            return &m_ptr;
        }

        T* operator->() const
        {
            return m_ptr;
        }

        void Reset()
        {
            if (m_ptr != nullptr)
            {
                m_ptr->Release();
                m_ptr = nullptr;
            }
        }

    private:
        T* m_ptr{nullptr};
    };

    void SetNativeDialogInitialPath(IFileDialog* dialog, const std::filesystem::path& seedPath)
    {
        if (seedPath.empty())
        {
            return;
        }

        std::filesystem::path initialDirectory = seedPath;
        if (seedPath.has_filename())
        {
            initialDirectory = seedPath.parent_path();
        }

        const std::wstring folder = initialDirectory.wstring();
        ScopedComPtr<IShellItem> shellFolder{};
        if (SUCCEEDED(SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(shellFolder.Put()))))
        {
            dialog->SetDefaultFolder(shellFolder.Get());
            dialog->SetFolder(shellFolder.Get());
        }

        if (seedPath.has_filename())
        {
            const std::wstring fileName = seedPath.filename().wstring();
            if (!fileName.empty())
            {
                dialog->SetFileName(fileName.c_str());
            }
        }
    }
#endif

#if !HIVE_PLATFORM_WINDOWS
    std::string QuoteShellArgument(const std::string& value)
    {
        std::string escaped{"'"};
        for (const char ch : value)
        {
            if (ch == '\'')
            {
                escaped += "'\"'\"'";
            }
            else
            {
                escaped.push_back(ch);
            }
        }
        escaped.push_back('\'');
        return escaped;
    }

    bool RunShellDialogCommand(const std::string& command, std::string* output)
    {
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe == nullptr)
        {
            return false;
        }

        std::string result;
        char buffer[512];
        while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr)
        {
            result += buffer;
        }

        const int status = pclose(pipe);
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        {
            result.pop_back();
        }

        if (status != 0 || result.empty())
        {
            return false;
        }

        *output = result;
        return true;
    }
#endif

    NativePathPickerResult ShowNativePathPicker(NativePathPickerMode mode, const std::filesystem::path& seedPath,
                                                std::filesystem::path* selectedPath)
    {
#if HIVE_PLATFORM_WINDOWS
        ScopedComApartment apartment{};
        if (!apartment.IsReady())
        {
            return NativePathPickerResult::UNAVAILABLE;
        }

        ScopedComPtr<IFileDialog> dialog{};
        const CLSID dialogClassId = mode == NativePathPickerMode::SCENE_FILE_SAVE ? CLSID_FileSaveDialog
                                                                                  : CLSID_FileOpenDialog;
        HRESULT hr = CoCreateInstance(dialogClassId, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.Put()));
        if (FAILED(hr))
        {
            return NativePathPickerResult::FAILED;
        }

        DWORD options = 0;
        if (FAILED(dialog->GetOptions(&options)))
        {
            return NativePathPickerResult::FAILED;
        }

        options |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
        if (mode == NativePathPickerMode::DIRECTORY)
        {
            options |= FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
            dialog->SetTitle(L"Select Project Directory");
        }
        else if (mode == NativePathPickerMode::PROJECT_FILE)
        {
            options |= FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST;
            dialog->SetTitle(L"Select Hive Project");
            const COMDLG_FILTERSPEC filters[] = {
                {L"Hive Project", L"project.hive"},
                {L"Hive Project Files", L"*.hive"},
                {L"All Files", L"*.*"},
            };
            dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
            dialog->SetFileTypeIndex(1);
        }
        else if (mode == NativePathPickerMode::SCENE_FILE_OPEN)
        {
            options |= FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST;
            dialog->SetTitle(L"Open Hive Scene");
            const COMDLG_FILTERSPEC filters[] = {
                {L"Hive Scene", L"*.hscene"},
                {L"All Files", L"*.*"},
            };
            dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
            dialog->SetFileTypeIndex(1);
        }
        else
        {
            options |= FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT;
            dialog->SetTitle(L"Save Hive Scene");
            const COMDLG_FILTERSPEC filters[] = {
                {L"Hive Scene", L"*.hscene"},
                {L"All Files", L"*.*"},
            };
            dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
            dialog->SetFileTypeIndex(1);
            dialog->SetDefaultExtension(L"hscene");
        }

        if (FAILED(dialog->SetOptions(options)))
        {
            return NativePathPickerResult::FAILED;
        }

        SetNativeDialogInitialPath(dialog.Get(), seedPath);

        hr = dialog->Show(nullptr);
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            return NativePathPickerResult::CANCELLED;
        }

        if (FAILED(hr))
        {
            return NativePathPickerResult::FAILED;
        }

        ScopedComPtr<IShellItem> result{};
        if (FAILED(dialog->GetResult(result.Put())))
        {
            return NativePathPickerResult::FAILED;
        }

        PWSTR rawPath = nullptr;
        hr = result->GetDisplayName(SIGDN_FILESYSPATH, &rawPath);
        if (FAILED(hr) || rawPath == nullptr)
        {
            return NativePathPickerResult::FAILED;
        }

        *selectedPath = std::filesystem::path{rawPath};
        CoTaskMemFree(rawPath);
        return NativePathPickerResult::SUCCESS;
#else
        std::string title = "Select Hive Project";
        std::string filter = "*.hive|Hive Project";
        if (mode == NativePathPickerMode::DIRECTORY)
        {
            title = "Select Project Directory";
        }
        else if (mode == NativePathPickerMode::SCENE_FILE_OPEN)
        {
            title = "Open Hive Scene";
            filter = "*.hscene|Hive Scene";
        }
        else if (mode == NativePathPickerMode::SCENE_FILE_SAVE)
        {
            title = "Save Hive Scene";
            filter = "*.hscene|Hive Scene";
        }

        const std::string seed = (seedPath.empty() ? GetHomeDirectory() : seedPath).generic_string();

        std::string selected{};
        std::string zenityCommand = "zenity --file-selection ";
        if (mode == NativePathPickerMode::DIRECTORY)
        {
            zenityCommand += "--directory ";
        }
        else if (mode == NativePathPickerMode::SCENE_FILE_SAVE)
        {
            zenityCommand += "--save --confirm-overwrite ";
        }
        zenityCommand += "--title=" + QuoteShellArgument(title) + " ";
        if (!seed.empty())
        {
            zenityCommand += "--filename=" + QuoteShellArgument(seed) + " ";
        }
        if (mode == NativePathPickerMode::PROJECT_FILE)
        {
            zenityCommand += "--file-filter=" + QuoteShellArgument("Hive Project | project.hive *.hive") + " ";
        }
        else if (mode == NativePathPickerMode::SCENE_FILE_OPEN || mode == NativePathPickerMode::SCENE_FILE_SAVE)
        {
            zenityCommand += "--file-filter=" + QuoteShellArgument("Hive Scene | *.hscene") + " ";
        }

        if (RunShellDialogCommand(zenityCommand, &selected))
        {
            *selectedPath = std::filesystem::path{selected};
            return NativePathPickerResult::SUCCESS;
        }

        std::string kdialogCommand;
        if (mode == NativePathPickerMode::DIRECTORY)
        {
            kdialogCommand = "kdialog --getexistingdirectory " + QuoteShellArgument(seed);
        }
        else if (mode == NativePathPickerMode::SCENE_FILE_SAVE)
        {
            kdialogCommand =
                "kdialog --getsavefilename " + QuoteShellArgument(seed) + " " + QuoteShellArgument(filter);
        }
        else
        {
            kdialogCommand =
                "kdialog --getopenfilename " + QuoteShellArgument(seed) + " " + QuoteShellArgument(filter);
        }
        if (RunShellDialogCommand(kdialogCommand, &selected))
        {
            *selectedPath = std::filesystem::path{selected};
            return NativePathPickerResult::SUCCESS;
        }

        return NativePathPickerResult::UNAVAILABLE;
#endif
    }
#endif

#if HIVE_MODE_EDITOR
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
    }

    void ResetSceneEditorState(LauncherState& state)
    {
        state.m_selection.Clear();
        state.m_undo = std::make_unique<forge::UndoStack>();
        state.m_pendingEditorAction = PendingEditorAction::NONE;
        state.m_pendingScenePath.clear();
        state.m_openUnsavedScenePopup = false;
        state.m_scenePromptError.clear();
    }

    void ClearWorldEntities(queen::World& world)
    {
        std::vector<queen::Entity> roots;
        roots.reserve(world.EntityCount());

        world.ForEachArchetype([&](auto& archetype) {
            for (uint32_t row = 0; row < archetype.EntityCount(); ++row)
            {
                const queen::Entity entity = archetype.GetEntity(row);
                if (world.GetParent(entity).IsNull())
                {
                    roots.push_back(entity);
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

        std::vector<queen::Entity> leftovers;
        world.ForEachArchetype([&](auto& archetype) {
            for (uint32_t row = 0; row < archetype.EntityCount(); ++row)
            {
                leftovers.push_back(archetype.GetEntity(row));
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

        const std::string relativeString = relative.generic_string();
        return !relativeString.empty() && relativeString != ".." && !relativeString.starts_with("../");
    }

    std::filesystem::path ResolveScenePath(const waggle::ProjectManager& projectManager, wax::StringView relativePath)
    {
        if (relativePath.IsEmpty())
        {
            return {};
        }

        return std::filesystem::path{projectManager.Paths().m_assets.CStr()} /
               std::string{relativePath.Data(), relativePath.Size()};
    }

    std::string MakeSceneRelativePath(const waggle::ProjectManager& projectManager, const std::filesystem::path& scenePath)
    {
        std::error_code ec;
        const std::filesystem::path relative =
            std::filesystem::relative(scenePath, std::filesystem::path{projectManager.Paths().m_assets.CStr()}, ec);
        if (ec)
        {
            return scenePath.generic_string();
        }

        const std::string value = relative.generic_string();
        if (value.empty() || value.starts_with(".."))
        {
            return scenePath.generic_string();
        }
        return value;
    }

    void ResetSceneRecoveryPrompt(LauncherState& state)
    {
        state.m_recoverySceneRelative.clear();
        state.m_recoveryPromptError.clear();
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

    bool WriteTextFile(const std::filesystem::path& path, const std::string& content)
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

        const size_t written = std::fwrite(content.data(), 1, content.size(), file);
        std::fclose(file);
        return written == content.size();
    }

    bool ReadTextFile(const std::filesystem::path& path, std::string* content)
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

        std::vector<char> buffer(static_cast<size_t>(fileSize) + 1, '\0');
        const size_t bytesRead = std::fread(buffer.data(), 1, static_cast<size_t>(fileSize), file);
        std::fclose(file);
        if (bytesRead != static_cast<size_t>(fileSize))
        {
            return false;
        }

        *content = std::string{buffer.data(), bytesRead};
        return true;
    }

    std::string GetSceneRecoverySourceRelative(LauncherState& state)
    {
        if (!state.m_currentSceneRelative.empty())
        {
            return state.m_currentSceneRelative;
        }

        if (state.m_project == nullptr)
        {
            return {};
        }

        std::filesystem::path preferredScenePath{};
        if (!state.m_currentScenePath.empty())
        {
            preferredScenePath = std::filesystem::path{state.m_currentScenePath};
        }
        else
        {
            const wax::StringView startupScene = state.m_project->Project().StartupSceneRelative();
            preferredScenePath = !startupScene.IsEmpty() ? ResolveScenePath(*state.m_project, startupScene)
                                                         : GetProjectAssetsDirectory(state) / "scenes" / "main.hscene";
        }

        return MakeSceneRelativePath(*state.m_project, preferredScenePath);
    }

    bool WriteSceneRecoveryMetadata(LauncherState& state, const std::string& sceneRelative)
    {
        auto& alloc = state.m_alloc.Get();
        nectar::HiveDocument document{alloc};
        document.SetValue("recovery", "scene", nectar::HiveValue::MakeString(alloc, {sceneRelative.c_str(), sceneRelative.size()}));
        const wax::String content = nectar::HiveWriter::Write(document, alloc);
        return WriteTextFile(GetSceneRecoveryMetadataPath(state), std::string{content.CStr(), content.Size()});
    }

    bool LoadSceneRecoveryMetadata(LauncherState& state, std::string* sceneRelative)
    {
        std::string content{};
        if (!ReadTextFile(GetSceneRecoveryMetadataPath(state), &content))
        {
            return false;
        }

        auto& alloc = state.m_alloc.Get();
        const nectar::HiveParseResult parseResult =
            nectar::HiveParser::Parse(wax::StringView{content.c_str(), content.size()}, alloc);
        if (!parseResult.Success())
        {
            return false;
        }

        const wax::StringView recoveryScene = parseResult.m_document.GetString("recovery", "scene");
        if (recoveryScene.IsEmpty())
        {
            return false;
        }

        *sceneRelative = std::string{recoveryScene.Data(), recoveryScene.Size()};
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

        std::string sceneRelative{};
        if (!LoadSceneRecoveryMetadata(state, &sceneRelative))
        {
            ClearSceneRecoveryArtifacts(state);
            return;
        }

        state.m_recoverySceneRelative = sceneRelative;
        state.m_recoveryPromptError.clear();
        state.m_openRecoveryScenePopup = true;
    }

    std::filesystem::path NormalizeScenePath(const LauncherState& state, std::filesystem::path scenePath, bool allowImplicitExtension)
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

    bool ResolveSceneAssetPath(const LauncherState& state, const std::filesystem::path& rawScenePath, bool requireExisting,
                               std::filesystem::path* resolvedScenePath, std::string* error)
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
        if (!state.m_currentScenePath.empty())
        {
            return std::filesystem::path{state.m_currentScenePath};
        }

        const wax::StringView startupScene = state.m_project->Project().StartupSceneRelative();
        if (!startupScene.IsEmpty())
        {
            return ResolveScenePath(*state.m_project, startupScene);
        }

        return GetProjectAssetsDirectory(state) / "scenes" / "main.hscene";
    }

    bool LoadEditorScene(waggle::EngineContext& ctx, LauncherState& state, const std::filesystem::path& scenePath);

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

    bool LoadEditorSceneAsset(waggle::EngineContext& ctx, LauncherState& state, const std::filesystem::path& rawScenePath)
    {
        std::filesystem::path scenePath{};
        std::string error{};
        if (!ResolveSceneAssetPath(state, rawScenePath, true, &scenePath, &error))
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to open scene '{}': {}", rawScenePath.generic_string(), error);
            return false;
        }

        if (!LoadEditorScene(ctx, state, scenePath))
        {
            return false;
        }

        hive::LogInfo(LOG_LAUNCHER, "Scene opened: {}", scenePath.generic_string());
        return true;
    }

    bool SaveEditorSceneToPath(waggle::EngineContext& ctx, LauncherState& state, const std::filesystem::path& rawScenePath)
    {
        std::filesystem::path scenePath{};
        std::string error{};
        if (!ResolveSceneAssetPath(state, rawScenePath, false, &scenePath, &error))
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to save scene '{}': {}", rawScenePath.generic_string(), error);
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(scenePath.parent_path(), ec);
        if (ec)
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to create scene directory: {}", scenePath.parent_path().generic_string());
            return false;
        }

        const std::string scenePathString = scenePath.generic_string();
        if (!forge::SaveScene(*ctx.m_world, state.m_componentRegistry, scenePathString.c_str()))
        {
            return false;
        }

        state.m_currentScenePath = scenePathString;
        state.m_currentSceneRelative = MakeSceneRelativePath(*state.m_project, scenePath);
        state.m_sceneAutosaveElapsedSeconds = 0.0;
        state.m_sceneDirty = false;
        ClearSceneRecoveryArtifacts(state);
        hive::LogInfo(LOG_LAUNCHER, "Scene saved: {}", scenePathString);
        return true;
    }

    bool SaveSceneRecoverySnapshot(waggle::EngineContext& ctx, LauncherState& state)
    {
        if (state.m_project == nullptr || !state.m_sceneDirty)
        {
            return false;
        }

        const std::string sceneRelative = GetSceneRecoverySourceRelative(state);
        if (sceneRelative.empty())
        {
            return false;
        }

        const std::filesystem::path recoveryDirectory = GetSceneRecoveryDirectory(state);
        std::error_code ec;
        std::filesystem::create_directories(recoveryDirectory, ec);
        if (ec)
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to create recovery directory: {}", recoveryDirectory.generic_string());
            return false;
        }

        const std::filesystem::path recoveryScenePath = GetSceneRecoveryScenePath(state);
        const std::string recoveryScenePathString = recoveryScenePath.generic_string();
        if (!forge::SaveScene(*ctx.m_world, state.m_componentRegistry, recoveryScenePathString.c_str()))
        {
            return false;
        }

        if (!WriteSceneRecoveryMetadata(state, sceneRelative))
        {
            hive::LogWarning(LOG_LAUNCHER, "Failed to write recovery metadata: {}",
                             GetSceneRecoveryMetadataPath(state).generic_string());
            return false;
        }

        hive::LogInfo(LOG_LAUNCHER, "Scene autosaved for recovery: {}", sceneRelative);
        return true;
    }

    std::string CaptureSceneBackup(queen::World& world, const queen::ComponentRegistry<256>& registry)
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
                            const std::string& backupScene)
    {
        if (backupScene.empty())
        {
            return false;
        }

        const auto result = queen::WorldDeserializer::Deserialize(world, registry, backupScene.c_str());
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
        const bool hasPreviousScene = !state.m_currentScenePath.empty() || !state.m_currentSceneRelative.empty();
        const std::string previousSceneBackup =
            hasPreviousScene ? CaptureSceneBackup(*ctx.m_world, state.m_componentRegistry) : std::string{};
        const std::string previousScenePath = state.m_currentScenePath;
        const std::string previousSceneRelative = state.m_currentSceneRelative;
        const bool previousSceneDirty = state.m_sceneDirty;

        ClearWorldEntities(*ctx.m_world);
        ResetSceneEditorState(state);

        const std::string scenePathString = scenePath.generic_string();
        if (!forge::LoadScene(*ctx.m_world, state.m_componentRegistry, scenePathString.c_str()))
        {
            if (hasPreviousScene && RestoreSceneBackup(*ctx.m_world, state.m_componentRegistry, previousSceneBackup))
            {
                state.m_currentScenePath = previousScenePath;
                state.m_currentSceneRelative = previousSceneRelative;
                state.m_sceneDirty = previousSceneDirty;
                hive::LogWarning(LOG_LAUNCHER, "Scene load failed; restored the previous scene state.");
            }
            else if (hasPreviousScene && !previousSceneBackup.empty())
            {
                hive::LogWarning(LOG_LAUNCHER, "Scene load failed and the previous scene backup could not be restored.");
            }
            hive::LogWarning(LOG_LAUNCHER, "Failed to load scene: {}", scenePathString);
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
        const std::string recoveredSceneRelative = state.m_recoverySceneRelative;
        if (recoveredSceneRelative.empty())
        {
            return false;
        }

        const std::filesystem::path recoveryScenePath = GetSceneRecoveryScenePath(state);
        if (!LoadEditorScene(ctx, state, recoveryScenePath))
        {
            return false;
        }

        const std::filesystem::path restoredScenePath = NormalizeScenePath(state, std::filesystem::path{recoveredSceneRelative}, false);
        state.m_currentScenePath = restoredScenePath.generic_string();
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

    [[maybe_unused]] bool SaveEditorScene(waggle::EngineContext& ctx, LauncherState& state)
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

    [[maybe_unused]] bool SaveEditorSceneAs(waggle::EngineContext& ctx, LauncherState& state)
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
        state.m_currentScenePath = scenePath.generic_string();
        state.m_currentSceneRelative = MakeSceneRelativePath(*state.m_project, scenePath);
        state.m_sceneDirty = true;
        hive::LogInfo(LOG_LAUNCHER, "New scene initialized at {}", state.m_currentScenePath);
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
        const std::filesystem::path pendingScenePath = state.m_pendingScenePath;
        state.m_pendingEditorAction = PendingEditorAction::NONE;
        state.m_pendingScenePath.clear();

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
                                    const std::filesystem::path& pendingScenePath = {})
    {
        state.m_pendingScenePath = pendingScenePath.generic_string();

        if (!state.m_sceneDirty)
        {
            state.m_pendingEditorAction = action;
            ExecutePendingEditorAction(ctx, state);
            return;
        }

        state.m_pendingEditorAction = action;
        state.m_scenePromptError.clear();
        state.m_openUnsavedScenePopup = true;
    }

    [[maybe_unused]] bool OpenEditorSceneWithPicker(waggle::EngineContext& ctx, LauncherState& state)
    {
        std::filesystem::path selectedPath{};
        if (!PickScenePath(state, NativePathPickerMode::SCENE_FILE_OPEN, &selectedPath))
        {
            return false;
        }

        RequestPendingEditorAction(ctx, state, PendingEditorAction::OPEN_SCENE, selectedPath);
        return true;
    }

#endif // HIVE_MODE_EDITOR

    void TryLoadGameplayModule(waggle::EngineContext& ctx, LauncherState& state)
    {
        const auto root = std::string{state.m_project->Paths().m_root.CStr(), state.m_project->Paths().m_root.Size()};

#if HIVE_MODE_EDITOR
        state.m_assetsRoot = std::string{state.m_project->Paths().m_assets.CStr(), state.m_project->Paths().m_assets.Size()};
#endif

#if HIVE_PLATFORM_WINDOWS
        const std::string dllPath = root + "/gameplay.dll";
#else
        const std::string dllPath = root + "/gameplay.so";
#endif

        std::error_code dllEc;
        if (std::filesystem::exists(dllPath, dllEc) && !dllEc)
        {
            if (state.m_gameplay.Load(dllPath.c_str()))
            {
                if (!state.m_gameplay.Register(*ctx.m_world))
                {
                    hive::LogWarning(LOG_LAUNCHER, "Gameplay DLL Register() failed");
                }
            }
            else
            {
                hive::LogWarning(LOG_LAUNCHER, "Failed to load gameplay DLL: {}", state.m_gameplay.GetError());
            }
        }
        else
        {
            hive::LogInfo(LOG_LAUNCHER, "No gameplay DLL found at {}", dllPath);
        }
    }

    bool OpenProject(waggle::EngineContext& ctx, LauncherState& state, const std::filesystem::path& requestedPath)
    {
        if (state.m_project == nullptr)
        {
            return false;
        }

        if (state.m_projectOpen)
        {
#if HIVE_MODE_EDITOR
            SetHubStatus(state.m_hub, true, "A project is already open in this launcher session.");
#endif
            return false;
        }

        std::filesystem::path resolvedPath{};
        if (!ResolveProjectFilePath(requestedPath, &resolvedPath))
        {
#if HIVE_MODE_EDITOR
            SetHubStatus(state.m_hub, true, "The selected path does not contain a valid project.hive file.");
#endif
            return false;
        }

        const std::string projectPath = resolvedPath.generic_string();
        const waggle::ProjectConfig projectConfig{
            .m_enableHotReload = HIVE_FEATURE_HOT_RELOAD != 0,
            .m_watcherIntervalMs = 500,
        };

        if (!state.m_project->Open({projectPath.c_str(), projectPath.size()}, projectConfig))
        {
#if HIVE_MODE_EDITOR
            SetHubStatus(state.m_hub, true, "Failed to open the selected project.");
#endif
            hive::LogError(LOG_LAUNCHER, "Failed to open project: {}", projectPath);
            return false;
        }

        state.m_projectPath = projectPath;
        state.m_projectOpen = true;

        const auto& project = state.m_project->Project();
        hive::LogInfo(LOG_LAUNCHER, "Project '{}' v{}", std::string{project.Name().Data(), project.Name().Size()},
                      std::string{project.Version().Data(), project.Version().Size()});

        ctx.m_world->InsertResource(waggle::ProjectContext{state.m_project});

        if (ctx.m_window != nullptr)
        {
            const std::string windowTitle = BuildWindowTitle(projectPath);
            terra::SetWindowTitle(ctx.m_window, windowTitle.c_str());
        }

#if HIVE_MODE_EDITOR
        ResetSceneEditorState(state);
        state.m_currentScenePath.clear();
        state.m_currentSceneRelative.clear();

        const wax::StringView startupScene = state.m_project->Project().StartupSceneRelative();
        if (!startupScene.IsEmpty())
        {
            const std::filesystem::path startupScenePath = ResolveScenePath(*state.m_project, startupScene);
            if (!LoadEditorScene(ctx, state, startupScenePath))
            {
                hive::LogWarning(LOG_LAUNCHER, "Failed to load startup scene: {}", startupScenePath.generic_string());
            }
        }
        QueueSceneRecoveryPrompt(state);
#endif
        TryLoadGameplayModule(ctx, state);

#if HIVE_MODE_EDITOR
        SetHubStatus(state.m_hub, false, "Project ready.");
#endif

        if (state.m_exitAfterSetup)
        {
            ctx.m_app->RequestStop();
        }

        return true;
    }

#if HIVE_MODE_EDITOR
    [[maybe_unused]] bool CreateProjectFromHub(waggle::EngineContext& ctx, LauncherState& state)
    {
        ProjectHubState& hub = state.m_hub;
        const std::string projectName = TrimmedCopy(hub.m_createName.data());
        if (!IsProjectNameValid(projectName))
        {
            SetHubStatus(hub, true, "Project name must use only letters, numbers, '_' or '-'.");
            return false;
        }

        const std::string version = TrimmedCopy(hub.m_createVersion.data()).empty()
                                        ? std::string{"0.1.0"}
                                        : TrimmedCopy(hub.m_createVersion.data());
        if (hub.m_engineRoot.empty())
        {
            SetHubStatus(hub, true, "Unable to locate the HiveEngine root. Set HIVE_ENGINE_DIR or run from an engine build.");
            return false;
        }

        if (!hub.m_supportEditor && !hub.m_supportGame && !hub.m_supportHeadless)
        {
            SetHubStatus(hub, true, "Enable at least one runtime mode before creating the project.");
            return false;
        }

        const std::filesystem::path targetRoot = BuildCreateTargetPath(hub, projectName);
        if (targetRoot.empty())
        {
            SetHubStatus(hub, true, "Choose a destination directory before creating the project.");
            return false;
        }

        std::error_code ec;
        if (std::filesystem::exists(targetRoot, ec) && !ec)
        {
            if (!std::filesystem::is_directory(targetRoot, ec))
            {
                SetHubStatus(hub, true, "The destination already exists and is not a directory.");
                return false;
            }

            std::filesystem::directory_iterator end{};
            std::filesystem::directory_iterator it{targetRoot, ec};
            if (!ec && it != end)
            {
                SetHubStatus(hub, true, "The destination directory is not empty.");
                return false;
            }
        }

        const bool wantsGraphics = hub.m_supportEditor || hub.m_supportGame;
        const std::string engineRoot = hub.m_engineRoot.generic_string();
        const std::string targetRootString = targetRoot.generic_string();
        const std::string presetBase = GetPresetBase(hub.m_toolchain);
        const std::string runtimeBackend = wantsGraphics ? "vulkan" : "";

        waggle::ProjectScaffoldConfig scaffoldConfig{};
        scaffoldConfig.m_cmake.m_projectName = {projectName.c_str(), projectName.size()};
        scaffoldConfig.m_cmake.m_projectRoot = {targetRootString.c_str(), targetRootString.size()};
        scaffoldConfig.m_cmake.m_enginePath = {engineRoot.c_str(), engineRoot.size()};
        scaffoldConfig.m_cmake.m_linkSwarm = wantsGraphics;
        scaffoldConfig.m_cmake.m_linkTerra = wantsGraphics;
        scaffoldConfig.m_cmake.m_linkAntennae = wantsGraphics;
        scaffoldConfig.m_projectVersion = {version.c_str(), version.size()};
        scaffoldConfig.m_runtimeBackend = {runtimeBackend.c_str(), runtimeBackend.size()};
        scaffoldConfig.m_presetBase = {presetBase.c_str(), presetBase.size()};
        scaffoldConfig.m_supportEditor = hub.m_supportEditor;
        scaffoldConfig.m_supportGame = hub.m_supportGame;
        scaffoldConfig.m_supportHeadless = hub.m_supportHeadless;

        if (!waggle::ProjectScaffolder::WriteToProject(scaffoldConfig, state.m_alloc.Get()))
        {
            SetHubStatus(hub, true, "Project files could not be generated.");
            return false;
        }

        RefreshProjectHub(state);
        CopyStringToBuffer((targetRoot / "project.hive").generic_string(), hub.m_openPath.data(), hub.m_openPath.size());

        return OpenProject(ctx, state, targetRoot / "project.hive");
    }
#endif

    class LauncherProcessSession
    {
    public:
        int Run(int argc, char* argv[])
        {
            LauncherCommandLine commandLine{};
            if (!TryParseCommandLine(argc, argv, &commandLine))
            {
                return 1;
            }

            return Run(commandLine);
        }

        private:
            int Run(const LauncherCommandLine& commandLine)
            {
                LauncherState state{};
                state.m_exitAfterSetup = commandLine.m_exitAfterSetup;

                waggle::EngineMode requestedMode{};
                if (!ResolveLauncherMode(commandLine, &requestedMode))
                {
                    PrintUsage();
                    return 1;
                }

                std::error_code ec;
                std::filesystem::path startupProjectPath = commandLine.m_projectPath;
                bool showProjectHub = false;

#if HIVE_MODE_EDITOR
                if (startupProjectPath.empty() && requestedMode == waggle::EngineMode::EDITOR)
                {
                    showProjectHub = true;
                    InitializeProjectHub(state);
                }
#endif

            if (!showProjectHub)
            {
                if (startupProjectPath.empty())
                {
                    startupProjectPath = std::filesystem::current_path(ec) / "project.hive";
                }

                if (!ResolveProjectFilePath(startupProjectPath, &startupProjectPath))
                {
                    std::fprintf(stderr, "Error: project file not found: %s\n", startupProjectPath.string().c_str());
                    PrintUsage();
                    return 1;
                }

                state.m_projectPath = startupProjectPath.generic_string();
            }

            const std::string windowTitle =
                state.m_projectPath.empty() ? std::string{"HiveEngine Launcher"} : BuildWindowTitle(state.m_projectPath);

                waggle::EngineConfig config{};
                config.m_windowTitle = windowTitle.c_str();
#if HIVE_MODE_EDITOR
                config.m_windowWidth = 1920;
                config.m_windowHeight = 1080;
                config.m_deferWindow = true;
#endif
                config.m_mode = requestedMode;

                waggle::EngineCallbacks callbacks{};
            callbacks.m_onSetup = [](waggle::EngineContext& ctx, void* ud) -> bool {
                auto& s = *static_cast<LauncherState*>(ud);
                auto& alloc = s.m_alloc.Get();

                s.m_project = comb::New<waggle::ProjectManager>(alloc, alloc);
                ctx.m_world->InsertResource(waggle::AppContext{ctx.m_app});

#if HIVE_MODE_EDITOR
                RegisterSceneComponentTypes(s);
#endif

#if HIVE_MODE_EDITOR
                s.m_mainWindow = new forge::ForgeMainWindow;
                s.m_mainWindow->Initialize(ctx.m_world, &s.m_selection, &s.m_componentRegistry, s.m_undo.get());

                InitializeProjectHub(s);
                std::vector<forge::DiscoveredProject> hubProjects;
                for (const auto& p : s.m_hub.m_discoveredProjects)
                    hubProjects.push_back({p.m_name, p.m_version, p.m_path, p.m_isCurrentDirectory});

                if (s.m_projectPath.empty())
                {
                    s.m_mainWindow->ShowHub(hubProjects);
                }
                else
                {
                    waggle::CreateWindowAndRenderer(ctx, s.m_windowTitle.c_str(), s.m_windowWidth, s.m_windowHeight);
                    s.m_mainWindow->ShowEditor();
                    if (ctx.m_renderContext != nullptr && ctx.m_window != nullptr)
                        s.m_mainWindow->AttachViewport(ctx.m_window, ctx.m_renderContext);
                    s.m_mainWindow->RefreshAll();
                }

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::hubProjectSelected,
                    [&ctx, &s](const QString& path) {
                        if (OpenProject(ctx, s, std::filesystem::path{path.toStdString()}))
                        {
                            waggle::CreateWindowAndRenderer(ctx, s.m_windowTitle.c_str(), s.m_windowWidth, s.m_windowHeight);
                            s.m_mainWindow->ShowEditor();
                            if (ctx.m_renderContext != nullptr && ctx.m_window != nullptr)
                                s.m_mainWindow->AttachViewport(ctx.m_window, ctx.m_renderContext);
                            s.m_mainWindow->RefreshAll();
                            if (!s.m_assetsRoot.empty())
                                s.m_mainWindow->SetAssetsRoot(s.m_assetsRoot.c_str());
                        }
                    });

                QObject::connect(s.m_mainWindow, &forge::ForgeMainWindow::editorCloseRequested,
                    [&ctx]() {
                        ctx.m_app->RequestStop();
                    });

                s.m_mainWindow->show();
#endif

                if (!s.m_projectPath.empty() && !OpenProject(ctx, s, std::filesystem::path{s.m_projectPath}))
                {
                    comb::Delete(alloc, s.m_project);
                    s.m_project = nullptr;
                    return false;
                }

                if (s.m_exitAfterSetup)
                {
                    ctx.m_app->RequestStop();
                }

                return true;
            };

            callbacks.m_onFrame = [](waggle::EngineContext& ctx, void* ud) {
                auto& s = *static_cast<LauncherState*>(ud);
                if (s.m_project != nullptr)
                {
                    s.m_project->Update();
                }

#if HIVE_MODE_EDITOR
                if (s.m_mainWindow != nullptr)
                {
                    s.m_mainWindow->RenderFrame();
                    QApplication::processEvents();
                }
#elif HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
                if (ctx.m_renderContext != nullptr)
                {
                    swarm::DrawPipeline(ctx.m_renderContext);
                }
#endif
            };

            callbacks.m_onShutdown = [](waggle::EngineContext& ctx, void* ud) {
                auto& s = *static_cast<LauncherState*>(ud);

#if HIVE_MODE_EDITOR
                delete s.m_mainWindow;
                s.m_mainWindow = nullptr;
#endif

                if (s.m_gameplay.IsRegistered())
                {
                    s.m_gameplay.Unregister(*ctx.m_world);
                }

                if (s.m_projectOpen && s.m_project != nullptr)
                {
                    ctx.m_world->RemoveResource<waggle::ProjectContext>();
                    s.m_project->Close();
                    s.m_projectOpen = false;
                }

                if (s.m_project != nullptr)
                {
                    comb::Delete(s.m_alloc.Get(), s.m_project);
                    s.m_project = nullptr;
                }

                // Do not FreeLibrary() during launcher teardown. The process exits
                // immediately after Run() returns, so leaving the module loaded lets
                // the OS reclaim it and avoids shutdown stalls in project DLL
                // destructors while the runtime is already coming down.
                s.m_gameplay.Abandon();
            };

            callbacks.m_userData = &state;
            return waggle::Run(config, callbacks);
        }
    };

#if HIVE_MODE_EDITOR
    [[maybe_unused]] std::string CompactPathForUi(const std::string& value, size_t maxChars)
    {
        if (value.size() <= maxChars || maxChars < 8)
        {
            return value;
        }

        const size_t tailChars = maxChars - 3;
        return "..." + value.substr(value.size() - tailChars);
    }

    [[maybe_unused]] bool PickPathIntoBuffer(ProjectHubState& hub, NativePathPickerMode mode, const std::filesystem::path& seedDirectory,
                            char* buffer, size_t bufferSize)
    {
        std::filesystem::path selectedPath{};
        const NativePathPickerResult result = ShowNativePathPicker(mode, seedDirectory, &selectedPath);
        switch (result)
        {
        case NativePathPickerResult::SUCCESS:
            CopyStringToBuffer(selectedPath.generic_string(), buffer, bufferSize);
            return true;

        case NativePathPickerResult::FAILED:
            SetHubStatus(hub, true, "The native path picker failed to open.");
            return false;

        case NativePathPickerResult::UNAVAILABLE:
            SetHubStatus(hub, true, "The native path picker is not available on this platform.");
            return false;

        case NativePathPickerResult::CANCELLED:
        default:
            return false;
        }
    }

#endif

} // anonymous namespace

int main(int argc, char* argv[])
{
    brood::ProcessRuntime runtime{};
#if HIVE_MODE_EDITOR
    QApplication qtApp{argc, argv};
    forge::ApplyForgeTheme();
#endif
    int result = 1;
    {
        LauncherProcessSession session{};
        result = session.Run(argc, argv);
    }
    // Runtime teardown is complete here. Avoid lingering during CRT static teardown.
    runtime.Exit(result);
}
