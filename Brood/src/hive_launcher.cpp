#include <hive/HiveConfig.h>
#include <hive/core/log.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <nectar/project/project_file.h>

#include <waggle/engine_runner.h>
#include <waggle/app_context.h>
#include <waggle/project/gameplay_module.h>
#include <waggle/project/project_context.h>
#include <waggle/project/project_manager.h>
#include <waggle/project/project_scaffolder.h>

#include <terra/terra.h>

#include <swarm/swarm.h>

#if HIVE_MODE_EDITOR
#include <queen/reflect/component_registry.h>

#include <terra/platform/glfw_terra.h>

#include <forge/asset_browser.h>
#include <forge/hierarchy_panel.h>
#include <forge/imgui_integration.h>
#include <forge/inspector_panel.h>
#include <forge/selection.h>
#include <forge/toolbar.h>
#include <forge/undo.h>

#include <imgui.h>

#include <imgui_internal.h>
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
#endif

    struct LauncherState
    {
        comb::ModuleAllocator m_alloc{"Launcher", size_t{1024} * 1024 * 1024};
        waggle::ProjectManager* m_project{nullptr};
        waggle::GameplayModule m_gameplay;
        std::string m_projectPath;
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
        bool m_firstFrame{true};
        bool m_imguiReady{false};
        swarm::ViewportRT* m_viewportRt{nullptr};
        void* m_viewportTexture{nullptr};
#endif
    };

#if HIVE_MODE_EDITOR
    void DrawProjectHub(waggle::EngineContext& ctx, LauncherState& state);
    void DrawEditor(waggle::EngineContext& ctx, LauncherState& state);
#endif

    struct LauncherCommandLine
    {
        std::filesystem::path m_projectPath{};
        bool m_forceHeadless{false};
        bool m_exitAfterSetup{false};
    };

    void PrintUsage()
    {
        std::fprintf(stderr,
                     "Usage: hive_launcher [--headless] [--exit-after-setup] [path/to/project.hive]\n"
                     "       Without a project path, the editor opens the project hub.\n");
    }

    bool TryParseCommandLine(int argc, char* argv[], LauncherCommandLine* commandLine)
    {
        for (int i = 1; i < argc; ++i)
        {
            const char* arg = argv[i];
            if (std::strcmp(arg, "--headless") == 0)
            {
                commandLine->m_forceHeadless = true;
                continue;
            }

            if (std::strcmp(arg, "--exit-after-setup") == 0)
            {
                commandLine->m_exitAfterSetup = true;
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

    std::filesystem::path GetPickerSeedDirectory(const char* rawPath)
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

    void SetNativeDialogInitialDirectory(IFileDialog* dialog, const std::filesystem::path& seedDirectory)
    {
        if (seedDirectory.empty())
        {
            return;
        }

        const std::wstring folder = seedDirectory.wstring();
        ScopedComPtr<IShellItem> shellFolder{};
        if (SUCCEEDED(SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(shellFolder.Put()))))
        {
            dialog->SetDefaultFolder(shellFolder.Get());
            dialog->SetFolder(shellFolder.Get());
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

    NativePathPickerResult ShowNativePathPicker(NativePathPickerMode mode, const std::filesystem::path& seedDirectory,
                                                std::filesystem::path* selectedPath)
    {
#if HIVE_PLATFORM_WINDOWS
        ScopedComApartment apartment{};
        if (!apartment.IsReady())
        {
            return NativePathPickerResult::UNAVAILABLE;
        }

        ScopedComPtr<IFileOpenDialog> dialog{};
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.Put()));
        if (FAILED(hr))
        {
            return NativePathPickerResult::FAILED;
        }

        DWORD options = 0;
        if (FAILED(dialog->GetOptions(&options)))
        {
            return NativePathPickerResult::FAILED;
        }

        options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
        if (mode == NativePathPickerMode::DIRECTORY)
        {
            options |= FOS_PICKFOLDERS;
            dialog->SetTitle(L"Select Project Directory");
        }
        else
        {
            dialog->SetTitle(L"Select Hive Project");
            const COMDLG_FILTERSPEC filters[] = {
                {L"Hive Project", L"project.hive"},
                {L"Hive Project Files", L"*.hive"},
                {L"All Files", L"*.*"},
            };
            dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
            dialog->SetFileTypeIndex(1);
        }

        if (FAILED(dialog->SetOptions(options)))
        {
            return NativePathPickerResult::FAILED;
        }

        SetNativeDialogInitialDirectory(dialog.Get(), seedDirectory);

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
        const std::string title =
            mode == NativePathPickerMode::DIRECTORY ? "Select Project Directory" : "Select Hive Project";
        const std::string seed =
            (seedDirectory.empty() ? GetHomeDirectory() : seedDirectory).generic_string();

        std::string selected{};
        std::string zenityCommand = "zenity --file-selection ";
        if (mode == NativePathPickerMode::DIRECTORY)
        {
            zenityCommand += "--directory ";
        }
        zenityCommand += "--title=" + QuoteShellArgument(title) + " ";
        if (!seed.empty())
        {
            zenityCommand += "--filename=" + QuoteShellArgument(seed) + " ";
        }

        if (RunShellDialogCommand(zenityCommand, &selected))
        {
            *selectedPath = std::filesystem::path{selected};
            return NativePathPickerResult::SUCCESS;
        }

        std::string kdialogCommand = mode == NativePathPickerMode::DIRECTORY
                                         ? "kdialog --getexistingdirectory " + QuoteShellArgument(seed)
                                         : "kdialog --getopenfilename " + QuoteShellArgument(seed) + " " +
                                               QuoteShellArgument("*.hive|Hive Project");
        if (RunShellDialogCommand(kdialogCommand, &selected))
        {
            *selectedPath = std::filesystem::path{selected};
            return NativePathPickerResult::SUCCESS;
        }

        return NativePathPickerResult::UNAVAILABLE;
#endif
    }
#endif

    void EnsureEditorProjectViewport(waggle::EngineContext& ctx, LauncherState& state)
    {
#if HIVE_MODE_EDITOR
        if (!state.m_imguiReady || ctx.m_renderContext == nullptr || state.m_viewportRt != nullptr)
        {
            return;
        }

        state.m_viewportRt = swarm::CreateViewportRT(ctx.m_renderContext, 1280, 720);
        if (state.m_viewportRt != nullptr)
        {
            state.m_viewportTexture = forge::ForgeRegisterViewportRT(ctx.m_renderContext, state.m_viewportRt);
        }
#else
        (void)ctx;
        (void)state;
#endif
    }

    void TryLoadGameplayModule(waggle::EngineContext& ctx, LauncherState& state)
    {
        const auto root = std::string{state.m_project->Paths().m_root.CStr(), state.m_project->Paths().m_root.Size()};

#if HIVE_MODE_EDITOR
        state.m_assetsRoot = root + "/assets";
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

        EnsureEditorProjectViewport(ctx, state);
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
    bool CreateProjectFromHub(waggle::EngineContext& ctx, LauncherState& state)
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

            std::error_code ec;
            std::filesystem::path startupProjectPath = commandLine.m_projectPath;
            bool showProjectHub = false;

#if HIVE_MODE_EDITOR
            if (startupProjectPath.empty() && !commandLine.m_forceHeadless)
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
            config.m_mode = commandLine.m_forceHeadless ? waggle::EngineMode::HEADLESS : waggle::EngineMode::EDITOR;
#elif HIVE_MODE_HEADLESS
            config.m_mode = waggle::EngineMode::HEADLESS;
#else
            config.m_mode = commandLine.m_forceHeadless ? waggle::EngineMode::HEADLESS : waggle::EngineMode::GAME;
#endif

            waggle::EngineCallbacks callbacks{};
            callbacks.m_onSetup = [](waggle::EngineContext& ctx, void* ud) -> bool {
                auto& s = *static_cast<LauncherState*>(ud);
                auto& alloc = s.m_alloc.Get();

                s.m_project = comb::New<waggle::ProjectManager>(alloc, alloc);
                ctx.m_world->InsertResource(waggle::AppContext{ctx.m_app});

#if HIVE_MODE_EDITOR
                if (ctx.m_renderContext != nullptr && ctx.m_window != nullptr)
                {
                    s.m_imguiReady = forge::ForgeImGuiInit(ctx.m_renderContext, terra::GetGlfwWindow(ctx.m_window));
                    if (!s.m_imguiReady)
                    {
                        hive::LogWarning(LOG_LAUNCHER, "Forge ImGui init failed");
                    }
                }
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
                if (ctx.m_renderContext != nullptr && s.m_imguiReady)
                {
                    if (s.m_projectOpen && s.m_viewportRt != nullptr)
                    {
                        swarm::BeginViewportRT(ctx.m_renderContext, s.m_viewportRt);
                        swarm::DrawPipeline(ctx.m_renderContext);
                        swarm::EndViewportRT(ctx.m_renderContext, s.m_viewportRt);
                    }

                    forge::ForgeImGuiNewFrame();
                    if (s.m_projectOpen)
                    {
                        DrawEditor(ctx, s);
                    }
                    else
                    {
                        DrawProjectHub(ctx, s);
                    }
                    forge::ForgeImGuiRender(ctx.m_renderContext);
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
                if (ctx.m_renderContext != nullptr)
                {
                    swarm::WaitForIdle(ctx.m_renderContext);
                    if (s.m_viewportTexture != nullptr)
                    {
                        forge::ForgeUnregisterViewportRT(s.m_viewportTexture);
                        s.m_viewportTexture = nullptr;
                    }
                    if (s.m_viewportRt != nullptr)
                    {
                        swarm::DestroyViewportRT(s.m_viewportRt);
                        s.m_viewportRt = nullptr;
                    }
                    if (s.m_imguiReady)
                    {
                        forge::ForgeImGuiShutdown(ctx.m_renderContext);
                        s.m_imguiReady = false;
                    }
                }
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

                // gameplay.Unload() is NOT called here: the World still has system
                // lambdas whose code lives in the DLL (e.g. FreeCamera). Calling
                // FreeLibrary now would unmap that code while the World still
                // references it. Instead, GameplayModule::~GameplayModule() will
                // call FreeLibrary after Run() returns and the World is destroyed.
            };

            callbacks.m_userData = &state;
            return waggle::Run(config, callbacks);
        }
    };

#if HIVE_MODE_EDITOR
    ImU32 ToU32(float r, float g, float b, float a = 1.0f)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4{r, g, b, a});
    }

    ImVec2 AddVec2(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2{lhs.x + rhs.x, lhs.y + rhs.y};
    }

    std::string CompactPathForUi(const std::string& value, size_t maxChars)
    {
        if (value.size() <= maxChars || maxChars < 8)
        {
            return value;
        }

        const size_t tailChars = maxChars - 3;
        return "..." + value.substr(value.size() - tailChars);
    }

    ImFont* GetHubHeadingFont()
    {
        ImGuiIO& io = ImGui::GetIO();
        return io.Fonts->Fonts.Size > 1 ? io.Fonts->Fonts[1] : ImGui::GetFont();
    }

    bool DrawHubSidebarItem(const char* id, const ImVec2& size, const ImVec4& accent, bool selected,
                            const char* eyebrow, const char* title, const char* body)
    {
        ImGui::PushID(id);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("card", size);

        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float rounding = 14.0f;

        const ImU32 background = selected ? ToU32(0.050f, 0.053f, 0.058f, 0.995f)
                                          : ToU32(0.036f, 0.038f, 0.042f, hovered ? 0.99f : 0.975f);
        const ImU32 border = selected ? ImGui::ColorConvertFloat4ToU32(ImVec4{accent.x, accent.y, accent.z, 0.72f})
                                      : hovered ? ToU32(0.13f, 0.14f, 0.17f, 0.98f)
                                                : ToU32(0.08f, 0.09f, 0.11f, 0.98f);
        const ImU32 accentFill =
            ImGui::ColorConvertFloat4ToU32(ImVec4{accent.x, accent.y, accent.z, selected ? 0.82f : 0.54f});
        drawList->AddRectFilled(pos, AddVec2(pos, size), background, rounding);
        drawList->AddRect(pos, AddVec2(pos, size), border, rounding, 0, selected ? 2.0f : 1.0f);
        drawList->AddRectFilled(AddVec2(pos, ImVec2{14.0f, 14.0f}), AddVec2(pos, ImVec2{18.0f, size.y - 14.0f}),
                                accentFill, 3.0f);

        ImFont* headingFont = GetHubHeadingFont();
        drawList->AddText(ImGui::GetFont(), 10.5f, AddVec2(pos, ImVec2{32.0f, 18.0f}),
                          ToU32(0.34f, 0.36f, 0.41f, 1.0f),
                          eyebrow);
        drawList->AddText(headingFont, 15.0f, AddVec2(pos, ImVec2{32.0f, 34.0f}), ToU32(0.86f, 0.88f, 0.91f),
                          title);
        drawList->AddText(ImGui::GetFont(), 12.0f, AddVec2(pos, ImVec2{32.0f, 56.0f}),
                          ToU32(0.44f, 0.47f, 0.52f, 1.0f),
                          body);

        ImGui::PopID();
        return clicked;
    }

    bool DrawHubChip(const char* id, const char* label, bool active, const ImVec4& accent)
    {
        ImGui::PushID(id);
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 size{textSize.x + 34.0f, 36.0f};
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("chip", size);
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 fill = active ? ImGui::ColorConvertFloat4ToU32(ImVec4{accent.x, accent.y, accent.z, 0.12f})
                                  : ToU32(0.027f, 0.029f, 0.033f, hovered ? 0.995f : 0.985f);
        const ImU32 border = active ? ImGui::ColorConvertFloat4ToU32(ImVec4{accent.x, accent.y, accent.z, 0.76f})
                                    : hovered ? ToU32(0.11f, 0.12f, 0.15f, 0.98f)
                                              : ToU32(0.07f, 0.08f, 0.10f, 0.98f);
        const ImU32 text = active ? ToU32(0.86f, 0.88f, 0.91f) : ToU32(0.47f, 0.50f, 0.55f);
        drawList->AddRectFilled(pos, AddVec2(pos, size), fill, 18.0f);
        drawList->AddRect(pos, AddVec2(pos, size), border, 18.0f, 0, active ? 2.0f : 1.0f);
        drawList->AddText(AddVec2(pos, ImVec2{17.0f, 10.0f}), text, label);
        ImGui::PopID();
        return clicked;
    }

    bool DrawHubActionButton(const char* id, const char* label, float width, bool primary)
    {
        ImGui::PushID(id);
        const ImVec2 size{width, 44.0f};
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("action", size);
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 fill = primary ? ToU32(0.11f, 0.14f, 0.18f, hovered ? 0.995f : 0.975f)
                                   : ToU32(0.027f, 0.029f, 0.033f, hovered ? 0.995f : 0.985f);
        const ImU32 border = primary ? ToU32(0.29f, 0.34f, 0.40f, 0.98f)
                                     : hovered ? ToU32(0.11f, 0.12f, 0.15f, 0.98f)
                                               : ToU32(0.07f, 0.08f, 0.10f, 0.98f);
        drawList->AddRectFilled(pos, AddVec2(pos, size), fill, 16.0f);
        drawList->AddRect(pos, AddVec2(pos, size), border, 16.0f, 0, primary ? 1.8f : 1.0f);
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        drawList->AddText(AddVec2(pos, ImVec2{(size.x - textSize.x) * 0.5f, (size.y - textSize.y) * 0.5f}),
                          primary ? ToU32(0.87f, 0.89f, 0.92f) : ToU32(0.50f, 0.53f, 0.58f), label);
        ImGui::PopID();
        return clicked;
    }

    bool DrawDiscoveredProjectCard(const LauncherDiscoveredProject& project, float width)
    {
        ImGui::PushID(project.m_path.c_str());
        const ImVec2 size{width, 62.0f};
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const std::string projectDirectory = std::filesystem::path{project.m_path}.parent_path().generic_string();
        const std::string compactDirectory = CompactPathForUi(projectDirectory, 48);
        ImGui::InvisibleButton("project", size);
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(pos, AddVec2(pos, size), ToU32(0.028f, 0.030f, 0.034f, hovered ? 0.995f : 0.985f), 10.0f);
        drawList->AddRect(pos, AddVec2(pos, size),
                          hovered ? ToU32(0.12f, 0.14f, 0.17f, 0.98f) : ToU32(0.07f, 0.08f, 0.10f, 0.98f), 10.0f);
        drawList->AddRectFilled(AddVec2(pos, ImVec2{16.0f, 12.0f}), AddVec2(pos, ImVec2{19.0f, size.y - 12.0f}),
                                ToU32(0.39f, 0.49f, 0.61f, hovered ? 0.88f : 0.74f), 2.0f);

        const float textX = 30.0f;
        const float titleY = 12.0f;
        const float pathY = 34.0f;
        const ImVec2 versionSize = ImGui::CalcTextSize(project.m_version.c_str());
        const float versionX = size.x - versionSize.x - 16.0f;

        if (project.m_isCurrentDirectory)
        {
            const ImVec2 badgeMin = AddVec2(pos, ImVec2{size.x - 132.0f, 8.0f});
            const ImVec2 badgeMax = AddVec2(pos, ImVec2{size.x - 14.0f, 27.0f});
            drawList->AddRectFilled(badgeMin, badgeMax, ToU32(0.052f, 0.055f, 0.061f, 0.995f), 9.0f);
            drawList->AddText(AddVec2(pos, ImVec2{size.x - 118.0f, 12.0f}), ToU32(0.53f, 0.58f, 0.64f), "CURRENT FOLDER");
        }

        drawList->AddText(GetHubHeadingFont(), 16.0f, AddVec2(pos, ImVec2{textX, titleY}), ToU32(0.88f, 0.90f, 0.93f),
                          project.m_name.c_str());
        drawList->AddText(AddVec2(pos, ImVec2{versionX, titleY + 2.0f}), ToU32(0.50f, 0.53f, 0.58f), project.m_version.c_str());
        drawList->AddText(AddVec2(pos, ImVec2{textX, pathY}), ToU32(0.42f, 0.45f, 0.50f), compactDirectory.c_str());
        ImGui::PopID();
        return clicked;
    }

    void DrawHubBackground(ImGuiViewport* viewport)
    {
        ImDrawList* drawList = ImGui::GetBackgroundDrawList(viewport);
        const ImVec2 topLeft = viewport->Pos;
        const ImVec2 bottomRight = AddVec2(viewport->Pos, viewport->Size);

        drawList->AddRectFilledMultiColor(topLeft, bottomRight, ToU32(0.012f, 0.013f, 0.015f),
                                          ToU32(0.014f, 0.015f, 0.017f), ToU32(0.010f, 0.011f, 0.013f),
                                          ToU32(0.009f, 0.010f, 0.012f));
        drawList->AddRectFilled(topLeft, AddVec2(topLeft, ImVec2{viewport->Size.x, 56.0f}), ToU32(0.018f, 0.019f, 0.022f, 0.94f));
        drawList->AddLine(AddVec2(topLeft, ImVec2{0.0f, 56.0f}), AddVec2(topLeft, ImVec2{viewport->Size.x, 56.0f}),
                          ToU32(0.07f, 0.08f, 0.10f, 0.90f), 1.0f);
    }

    void DrawStatusBanner(const ProjectHubState& hub, float width)
    {
        if (hub.m_statusMessage.empty())
        {
            return;
        }

        const ImVec2 size{width, 54.0f};
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec4 accent = hub.m_statusIsError ? ImVec4{0.76f, 0.33f, 0.35f, 1.0f} : ImVec4{0.44f, 0.60f, 0.72f, 1.0f};
        drawList->AddRectFilled(pos, AddVec2(pos, size), ToU32(0.030f, 0.032f, 0.036f, 0.995f), 12.0f);
        drawList->AddRect(pos, AddVec2(pos, size), ImGui::ColorConvertFloat4ToU32(accent), 18.0f);
        drawList->AddRectFilled(AddVec2(pos, ImVec2{16.0f, 14.0f}), AddVec2(pos, ImVec2{20.0f, 40.0f}),
                                ImGui::ColorConvertFloat4ToU32(accent), 4.0f);
        drawList->AddText(AddVec2(pos, ImVec2{34.0f, 18.0f}), ToU32(0.82f, 0.85f, 0.89f), hub.m_statusMessage.c_str());
        ImGui::Dummy(size);
    }

    bool PickPathIntoBuffer(ProjectHubState& hub, NativePathPickerMode mode, const std::filesystem::path& seedDirectory,
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

    void PushHubFieldStyle()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{22.0f, 18.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{16.0f, 14.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{12.0f, 14.0f});
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{0.028f, 0.030f, 0.034f, 0.995f});
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{0.040f, 0.042f, 0.047f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{0.047f, 0.050f, 0.056f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{0.10f, 0.11f, 0.13f, 0.98f});
    }

    void PopHubFieldStyle()
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(5);
    }

    void DrawProjectHub(waggle::EngineContext& ctx, LauncherState& state)
    {
        ProjectHubState& hub = state.m_hub;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        constexpr ImVec4 kOpenAccent{0.43f, 0.55f, 0.67f, 1.0f};
        constexpr ImVec4 kCreateAccent{0.62f, 0.52f, 0.38f, 1.0f};

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("Hive Project Hub", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar(3);

        DrawHubBackground(viewport);

        const float outerMargin = 28.0f;
        const float topY = viewport->Pos.y + 74.0f;
        const ImVec2 railPos{viewport->Pos.x + outerMargin, topY};
        const ImVec2 railSize{196.0f, viewport->Size.y - 108.0f};
        const float contentX = railPos.x + railSize.x + 16.0f;
        const float contentWidth = viewport->Pos.x + viewport->Size.x - contentX - outerMargin;
        const float contentHeight = viewport->Size.y - 108.0f;
        const ImVec2 panelPos{contentX, topY};
        const ImVec2 panelSize{contentWidth, contentHeight};

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(railPos, AddVec2(railPos, railSize), ToU32(0.020f, 0.021f, 0.024f, 0.995f), 18.0f);
        drawList->AddRect(railPos, AddVec2(railPos, railSize), ToU32(0.08f, 0.09f, 0.11f, 0.98f), 18.0f);
        drawList->AddRectFilled(panelPos, AddVec2(panelPos, panelSize), ToU32(0.018f, 0.019f, 0.022f, 0.995f), 18.0f);
        drawList->AddRect(panelPos, AddVec2(panelPos, panelSize), ToU32(0.08f, 0.09f, 0.11f, 0.98f), 18.0f);
        drawList->AddRectFilled(panelPos, AddVec2(panelPos, ImVec2{panelSize.x, 84.0f}), ToU32(0.023f, 0.024f, 0.028f, 0.995f),
                                18.0f);
        drawList->AddLine(AddVec2(panelPos, ImVec2{0.0f, 84.0f}), AddVec2(panelPos, ImVec2{panelSize.x, 84.0f}),
                          ToU32(0.08f, 0.09f, 0.11f, 0.98f), 1.0f);

        ImGui::SetCursorScreenPos(AddVec2(railPos, ImVec2{24.0f, 22.0f}));
        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.34f, 0.36f, 0.41f, 1.0f});
        ImGui::TextUnformatted("HIVE ENGINE");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2{0.0f, 6.0f});

        ImGui::PushFont(GetHubHeadingFont());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.86f, 0.88f, 0.91f, 1.0f});
        ImGui::TextUnformatted("Launcher");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.44f, 0.47f, 0.52f, 1.0f});
        ImGui::PushTextWrapPos(railPos.x + railSize.x - 22.0f);
        ImGui::TextUnformatted("Open a workspace or scaffold a standalone repository.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2{0.0f, 24.0f});
        if (DrawHubSidebarItem("OpenProjectCard", ImVec2{railSize.x - 48.0f, 76.0f}, kOpenAccent,
                               hub.m_page == LauncherHubPage::HUB_OPEN_EXISTING, "OPEN", "Open project",
                               "Existing workspace"))
        {
            hub.m_page = LauncherHubPage::HUB_OPEN_EXISTING;
        }

        ImGui::Dummy(ImVec2{0.0f, 10.0f});
        if (DrawHubSidebarItem("CreateProjectCard", ImVec2{railSize.x - 48.0f, 76.0f}, kCreateAccent,
                               hub.m_page == LauncherHubPage::HUB_CREATE_NEW, "CREATE", "New project",
                               "Standalone repository"))
        {
            hub.m_page = LauncherHubPage::HUB_CREATE_NEW;
        }

        const ImVec2 railInfoPos = AddVec2(railPos, ImVec2{24.0f, railSize.y - 104.0f});
        drawList->AddRectFilled(railInfoPos, AddVec2(railInfoPos, ImVec2{railSize.x - 48.0f, 80.0f}),
                                ToU32(0.028f, 0.030f, 0.034f, 0.995f), 14.0f);
        drawList->AddRect(railInfoPos, AddVec2(railInfoPos, ImVec2{railSize.x - 48.0f, 80.0f}),
                          ToU32(0.08f, 0.09f, 0.11f, 0.98f), 14.0f);
        ImGui::SetCursorScreenPos(AddVec2(railInfoPos, ImVec2{16.0f, 14.0f}));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.34f, 0.36f, 0.41f, 1.0f});
        ImGui::TextUnformatted("ENGINE ROOT");
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(AddVec2(railInfoPos, ImVec2{16.0f, 38.0f}));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.53f, 0.56f, 0.61f, 1.0f});
        if (!hub.m_engineRoot.empty())
        {
            ImGui::TextWrapped("%s", CompactPathForUi(hub.m_engineRoot.generic_string(), 28).c_str());
        }
        else
        {
            ImGui::TextWrapped("Engine root unavailable.");
        }
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        ImGui::SetCursorScreenPos(AddVec2(panelPos, ImVec2{28.0f, 22.0f}));
        ImGui::BeginGroup();

        const char* panelTitle =
            hub.m_page == LauncherHubPage::HUB_OPEN_EXISTING ? "Open Project" : "New Standalone Project";
        const char* panelBody = hub.m_page == LauncherHubPage::HUB_OPEN_EXISTING
                                    ? "Choose a .hive file, browse a folder, or open a detected workspace."
                                    : "Scaffold a standalone repository with presets, manifests, and a gameplay stub.";
        const float innerWidth = panelSize.x - 64.0f;

        ImGui::PushFont(GetHubHeadingFont());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.88f, 0.90f, 0.93f, 1.0f});
        ImGui::TextUnformatted(panelTitle);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.42f, 0.45f, 0.50f, 1.0f});
        ImGui::PushTextWrapPos(panelPos.x + panelSize.x - 28.0f);
        ImGui::TextUnformatted(panelBody);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(AddVec2(panelPos, ImVec2{28.0f, 96.0f}));
        DrawStatusBanner(hub, innerWidth);

        PushHubFieldStyle();
        const float contentAreaHeight = panelSize.y - (ImGui::GetCursorScreenPos().y - panelPos.y) - 24.0f;
        constexpr ImGuiChildFlags kHubPaddedChildFlags =
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
        constexpr ImGuiWindowFlags kHubNoScrollFlags =
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::BeginChild("HubContentArea", ImVec2{innerWidth, contentAreaHeight},
                          ImGuiChildFlags_AlwaysUseWindowPadding, kHubNoScrollFlags);

        if (hub.m_page == LauncherHubPage::HUB_OPEN_EXISTING)
        {
            const float recentWidth = (std::min)(390.0f, innerWidth * 0.36f);
            const float preferredSurfaceHeight = (std::clamp)(viewport->Size.y * 0.72f, 620.0f, 700.0f);
            const float surfaceHeight = (std::min)(contentAreaHeight - 10.0f, preferredSurfaceHeight);
            const float quickOpenHelpHeight = (std::clamp)(surfaceHeight * 0.24f, 164.0f, 208.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.015f, 0.016f, 0.019f, 0.995f});
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{0.08f, 0.09f, 0.11f, 0.98f});

            ImGui::BeginChild("RecentProjectsPane", ImVec2{recentWidth, surfaceHeight}, kHubPaddedChildFlags,
                              kHubNoScrollFlags);
            ImGui::PushFont(GetHubHeadingFont());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.87f, 0.89f, 0.92f, 1.0f});
            ImGui::TextUnformatted("Recent Projects");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.42f, 0.45f, 0.50f, 1.0f});
            ImGui::TextUnformatted("Detected around the engine repository.");
            ImGui::PopStyleColor();

            if (!hub.m_statusMessage.empty())
            {
                ImGui::Dummy(ImVec2{0.0f, 14.0f});
                DrawStatusBanner(hub, recentWidth - 24.0f);
            }
            else
            {
                ImGui::Dummy(ImVec2{0.0f, 14.0f});
            }

            const float footerHeight = 42.0f;
            const float listHeight = (std::max)(120.0f, ImGui::GetContentRegionAvail().y - footerHeight - 10.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.019f, 0.020f, 0.024f, 0.995f});
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{0.08f, 0.09f, 0.11f, 0.98f});
            ImGui::BeginChild("DetectedProjects", ImVec2{-1.0f, listHeight}, kHubPaddedChildFlags);
            if (hub.m_discoveredProjects.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.40f, 0.43f, 0.48f, 1.0f});
                ImGui::TextWrapped("No recent workspaces were detected near the engine root.");
                ImGui::PopStyleColor();
            }
            else
            {
                const float cardWidth = ImGui::GetContentRegionAvail().x;
                for (const LauncherDiscoveredProject& project : hub.m_discoveredProjects)
                {
                    if (DrawDiscoveredProjectCard(project, cardWidth))
                    {
                        CopyStringToBuffer(project.m_path, hub.m_openPath.data(), hub.m_openPath.size());
                        OpenProject(ctx, state, std::filesystem::path{project.m_path});
                    }
                    ImGui::Dummy(ImVec2{0.0f, 10.0f});
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);

            const float spacerHeight = (std::max)(0.0f, ImGui::GetContentRegionAvail().y - footerHeight);
            ImGui::Dummy(ImVec2{0.0f, spacerHeight});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.52f, 0.55f, 0.60f, 1.0f});
            ImGui::TextUnformatted(CompactPathForUi(hub.m_engineRoot.generic_string(), 36).c_str());
            ImGui::PopStyleColor();
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("QuickOpenPane", ImVec2{0.0f, surfaceHeight}, kHubPaddedChildFlags, kHubNoScrollFlags);
            ImGui::PushFont(GetHubHeadingFont());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.87f, 0.89f, 0.92f, 1.0f});
            ImGui::TextUnformatted("Quick Open");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.42f, 0.45f, 0.50f, 1.0f});
            ImGui::TextWrapped("Choose a .hive file, browse a folder, or jump into a recent workspace.");
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2{0.0f, 18.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.48f, 0.53f, 1.0f});
            ImGui::TextUnformatted("PROJECT PATH");
            ImGui::PopStyleColor();

            const float fileButtonWidth = 104.0f;
            const float folderButtonWidth = 88.0f;
            const float inputWidth = ImGui::GetContentRegionAvail().x - fileButtonWidth - folderButtonWidth - 24.0f;
            ImGui::SetNextItemWidth(inputWidth);
            ImGui::InputTextWithHint("##ProjectPath", "Paste a project.hive file or a project directory",
                                     hub.m_openPath.data(), hub.m_openPath.size());
            ImGui::SameLine();
            if (DrawHubActionButton("PickProjectFile", "Project File", fileButtonWidth, false))
            {
                PickPathIntoBuffer(hub, NativePathPickerMode::PROJECT_FILE, GetPickerSeedDirectory(hub.m_openPath.data()),
                                   hub.m_openPath.data(), hub.m_openPath.size());
            }
            ImGui::SameLine();
            if (DrawHubActionButton("PickProjectFolder", "Folder", folderButtonWidth, false))
            {
                PickPathIntoBuffer(hub, NativePathPickerMode::DIRECTORY, GetPickerSeedDirectory(hub.m_openPath.data()),
                                   hub.m_openPath.data(), hub.m_openPath.size());
            }

            ImGui::Dummy(ImVec2{0.0f, 18.0f});
            if (DrawHubActionButton("OpenProjectButton", "Open Project", 184.0f, true))
            {
                OpenProject(ctx, state, std::filesystem::path{TrimmedCopy(hub.m_openPath.data())});
            }

            ImGui::Dummy(ImVec2{0.0f, 20.0f});
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.019f, 0.020f, 0.024f, 0.995f});
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{0.08f, 0.09f, 0.11f, 0.98f});
            ImGui::BeginChild("QuickOpenHelp", ImVec2{-1.0f, quickOpenHelpHeight}, kHubPaddedChildFlags,
                              kHubNoScrollFlags);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.39f, 0.42f, 0.47f, 1.0f});
            ImGui::TextUnformatted("SUPPORTED INPUTS");
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2{0.0f, 8.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.75f, 0.78f, 0.82f, 1.0f});
            ImGui::TextUnformatted(".hive project manifest");
            ImGui::TextUnformatted("Project folder containing project.hive");
            ImGui::TextUnformatted("Recent workspace discovered near the engine");
            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
        }
        else
        {
            const float setupWidth = (std::min)(620.0f, innerWidth * 0.58f);
            const float preferredSurfaceHeight = (std::clamp)(viewport->Size.y * 0.72f, 620.0f, 700.0f);
            const float surfaceHeight = (std::min)(contentAreaHeight - 10.0f, preferredSurfaceHeight);
            const float setupContentTopPad = (std::clamp)((surfaceHeight - 520.0f) * 0.14f, 8.0f, 26.0f);
            const float rootPreviewHeight = (std::clamp)(surfaceHeight * 0.24f, 152.0f, 196.0f);
            const float filesPreviewHeight = (std::clamp)(surfaceHeight * 0.32f, 220.0f, 280.0f);
            const float previewContentTopPad =
                (std::clamp)((surfaceHeight - (rootPreviewHeight + filesPreviewHeight + 128.0f)) * 0.18f, 8.0f,
                             24.0f);
            const std::filesystem::path previewRoot =
                BuildCreateTargetPath(hub, TrimmedCopy(hub.m_createName.data()));
            const std::string previewRootLabel =
                previewRoot.empty() ? "Set a valid name and destination to continue."
                                    : CompactPathForUi(previewRoot.generic_string(), 56);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.015f, 0.016f, 0.019f, 0.995f});
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{0.08f, 0.09f, 0.11f, 0.98f});

            ImGui::BeginChild("ProjectSetupPane", ImVec2{setupWidth, surfaceHeight}, kHubPaddedChildFlags,
                              kHubNoScrollFlags);
            ImGui::Dummy(ImVec2{0.0f, setupContentTopPad});
            ImGui::PushFont(GetHubHeadingFont());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.87f, 0.89f, 0.92f, 1.0f});
            ImGui::TextUnformatted("Project Setup");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.42f, 0.45f, 0.50f, 1.0f});
            ImGui::TextWrapped("Scaffold a standalone repository with presets, manifests, and a gameplay stub.");
            ImGui::PopStyleColor();

            if (!hub.m_statusMessage.empty())
            {
                ImGui::Dummy(ImVec2{0.0f, 14.0f});
                DrawStatusBanner(hub, setupWidth - 24.0f);
            }
            else
            {
                ImGui::Dummy(ImVec2{0.0f, 14.0f});
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.48f, 0.53f, 1.0f});
            ImGui::TextUnformatted("PROJECT IDENTITY");
            ImGui::PopStyleColor();

            const float versionWidth = 160.0f;
            ImGui::SetNextItemWidth(setupWidth - 24.0f - versionWidth - 12.0f);
            ImGui::InputTextWithHint("##ProjectName", "Example: MyGame", hub.m_createName.data(),
                                     hub.m_createName.size());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(versionWidth);
            ImGui::InputTextWithHint("##CreateVersion", "0.1.0", hub.m_createVersion.data(), hub.m_createVersion.size());

            ImGui::Dummy(ImVec2{0.0f, 16.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.48f, 0.53f, 1.0f});
            ImGui::TextUnformatted("DESTINATION");
            ImGui::PopStyleColor();

            const float browseButtonWidth = 92.0f;
            const float defaultButtonWidth = 84.0f;
            const float destinationWidth = setupWidth - 24.0f - browseButtonWidth - defaultButtonWidth - 24.0f;
            ImGui::SetNextItemWidth(destinationWidth);
            ImGui::InputTextWithHint("##CreateDirectory", "Folder where the project repository will be generated",
                                     hub.m_createDirectory.data(), hub.m_createDirectory.size());
            ImGui::SameLine();
            if (DrawHubActionButton("BrowseCreateDirectory", "Browse", browseButtonWidth, false))
            {
                const std::filesystem::path seedDirectory = [&hub]() {
                    const std::filesystem::path seeded = GetPickerSeedDirectory(hub.m_createDirectory.data());
                    return seeded.empty() ? GetDefaultProjectsDirectory() : seeded;
                }();

                PickPathIntoBuffer(hub, NativePathPickerMode::DIRECTORY, seedDirectory, hub.m_createDirectory.data(),
                                   hub.m_createDirectory.size());
            }
            ImGui::SameLine();
            if (DrawHubActionButton("DefaultCreateDirectory", "Default", defaultButtonWidth, false))
            {
                const std::filesystem::path defaultDirectory = GetDefaultProjectsDirectory();
                if (!defaultDirectory.empty())
                {
                    CopyStringToBuffer(defaultDirectory.generic_string(), hub.m_createDirectory.data(),
                                       hub.m_createDirectory.size());
                }
            }

            ImGui::Dummy(ImVec2{0.0f, 22.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.48f, 0.53f, 1.0f});
            ImGui::TextUnformatted("RUNTIME MODES");
            ImGui::PopStyleColor();
            if (DrawHubChip("SupportEditor", "Editor", hub.m_supportEditor, kOpenAccent))
            {
                hub.m_supportEditor = !hub.m_supportEditor;
            }
            ImGui::SameLine();
            if (DrawHubChip("SupportGame", "Game", hub.m_supportGame, kOpenAccent))
            {
                hub.m_supportGame = !hub.m_supportGame;
            }
            ImGui::SameLine();
            if (DrawHubChip("SupportHeadless", "Headless", hub.m_supportHeadless, kCreateAccent))
            {
                hub.m_supportHeadless = !hub.m_supportHeadless;
            }

            ImGui::Dummy(ImVec2{0.0f, 22.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.48f, 0.53f, 1.0f});
            ImGui::TextUnformatted("COMPILER PRESET");
            ImGui::PopStyleColor();
            if (DrawHubChip("ToolchainLLVM", "LLVM", hub.m_toolchain == LauncherToolchainPreset::LLVM, kOpenAccent))
            {
                hub.m_toolchain = LauncherToolchainPreset::LLVM;
            }
            ImGui::SameLine();
#if HIVE_PLATFORM_WINDOWS
            if (DrawHubChip("ToolchainMSVC", "MSVC", hub.m_toolchain == LauncherToolchainPreset::MSVC,
                            kCreateAccent))
            {
                hub.m_toolchain = LauncherToolchainPreset::MSVC;
            }
#else
            if (DrawHubChip("ToolchainGCC", "GCC", hub.m_toolchain == LauncherToolchainPreset::GCC,
                            kCreateAccent))
            {
                hub.m_toolchain = LauncherToolchainPreset::GCC;
            }
#endif

            ImGui::Dummy(ImVec2{0.0f, 28.0f});
            ImGui::BeginDisabled(hub.m_engineRoot.empty());
            if (DrawHubActionButton("CreateProjectButton", "Create Project", 190.0f, true))
            {
                CreateProjectFromHub(ctx, state);
            }
            ImGui::EndDisabled();
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("GeneratedPipelinePane", ImVec2{0.0f, surfaceHeight}, kHubPaddedChildFlags,
                              kHubNoScrollFlags);
            ImGui::Dummy(ImVec2{0.0f, previewContentTopPad});
            ImGui::PushFont(GetHubHeadingFont());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.87f, 0.89f, 0.92f, 1.0f});
            ImGui::TextUnformatted("Generated Pipeline");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.42f, 0.45f, 0.50f, 1.0f});
            ImGui::TextWrapped("Preview the repository output before scaffolding.");
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2{0.0f, 14.0f});
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.019f, 0.020f, 0.024f, 0.995f});
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{0.08f, 0.09f, 0.11f, 0.98f});

            ImGui::BeginChild("PipelinePreviewRoot", ImVec2{-1.0f, rootPreviewHeight}, kHubPaddedChildFlags,
                              kHubNoScrollFlags);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.39f, 0.42f, 0.47f, 1.0f});
            ImGui::TextUnformatted("PROJECT ROOT");
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2{0.0f, 8.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.76f, 0.79f, 0.83f, 1.0f});
            ImGui::TextWrapped("%s", previewRootLabel.c_str());
            ImGui::Dummy(ImVec2{0.0f, 8.0f});
            ImGui::Text("Preset base: %s", GetPresetBase(hub.m_toolchain));
            ImGui::PopStyleColor();
            ImGui::EndChild();

            ImGui::Dummy(ImVec2{0.0f, 16.0f});
            ImGui::BeginChild("PipelinePreviewFiles", ImVec2{-1.0f, filesPreviewHeight}, kHubPaddedChildFlags,
                              kHubNoScrollFlags);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.39f, 0.42f, 0.47f, 1.0f});
            ImGui::TextUnformatted("GENERATED FILES");
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2{0.0f, 8.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.75f, 0.78f, 0.82f, 1.0f});
            ImGui::TextUnformatted("CMakeLists.txt");
            ImGui::TextUnformatted("CMakeUserPresets.json");
            ImGui::TextUnformatted("HiveProject.json");
            ImGui::TextUnformatted("project.hive");
            ImGui::TextUnformatted("src/gameplay.cpp");
            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
        }

        ImGui::EndChild();
        PopHubFieldStyle();
        ImGui::EndGroup();
        ImGui::End();
    }

    void SetupDefaultDockLayout(ImGuiID dockspaceId)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

        ImGuiID center = dockspaceId;
        const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
        const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
        const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);

        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Asset Browser", bottom);
        ImGui::DockBuilderDockWindow("Viewport", center);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void DrawEditor(waggle::EngineContext& ctx, LauncherState& state)
    {
        // Fullscreen dockspace
        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
                                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceId = ImGui::GetID("HiveEditorDockSpace");
        if (state.m_firstFrame)
        {
            SetupDefaultDockLayout(dockspaceId);
            state.m_firstFrame = false;
        }
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // Menu bar with toolbar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit"))
                    ctx.m_app->RequestStop();
                ImGui::EndMenu();
            }

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 2.f);

            const forge::ToolbarAction action = forge::DrawToolbarButtons(state.m_playState, state.m_gizmo);
            if (action.m_playPressed)
                state.m_playState = forge::PlayState::PLAYING;
            if (action.m_pausePressed)
                state.m_playState = forge::PlayState::PAUSED;
            if (action.m_stopPressed)
                state.m_playState = forge::PlayState::EDITING;

            ImGui::EndMenuBar();
        }

        ImGui::End(); // DockSpace

        // Hierarchy
        if (ImGui::Begin("Hierarchy"))
            forge::DrawHierarchyPanel(*ctx.m_world, state.m_selection);
        ImGui::End();

        // Inspector
        if (ImGui::Begin("Inspector"))
            forge::DrawInspectorPanel(*ctx.m_world, state.m_selection, state.m_componentRegistry, *state.m_undo);
        ImGui::End();

        // Asset Browser
        if (ImGui::Begin("Asset Browser"))
            forge::DrawAssetBrowser(state.m_assetsRoot.c_str());
        ImGui::End();

        // Viewport
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::Begin("Viewport"))
        {
            const ImVec2 size = ImGui::GetContentRegionAvail();
            if (state.m_viewportRt != nullptr && state.m_viewportTexture != nullptr && size.x > 0 && size.y > 0)
            {
                const uint32_t w = static_cast<uint32_t>(size.x);
                const uint32_t h = static_cast<uint32_t>(size.y);
                if (w != swarm::GetViewportRTWidth(state.m_viewportRt) ||
                    h != swarm::GetViewportRTHeight(state.m_viewportRt))
                {
                    forge::ForgeUnregisterViewportRT(state.m_viewportTexture);
                    swarm::ResizeViewportRT(state.m_viewportRt, w, h);
                    state.m_viewportTexture = forge::ForgeRegisterViewportRT(ctx.m_renderContext, state.m_viewportRt);
                }
                ImGui::Image(state.m_viewportTexture, size);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
#endif

} // anonymous namespace

int main(int argc, char* argv[])
{
    brood::ProcessRuntime runtime{};
    int result = 1;
    {
        LauncherProcessSession session{};
        result = session.Run(argc, argv);
    }
    // Runtime teardown is complete here. Avoid lingering during CRT static teardown.
    runtime.Exit(result);
}
