#include <comb/default_allocator.h>

#include <nectar/project/project_file.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <launcher/launcher_platform.h>

namespace brood::launcher
{

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

        std::sort(projects.begin(), projects.end(),
                  [](const LauncherDiscoveredProject& lhs, const LauncherDiscoveredProject& rhs) {
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

    void RefreshProjectHub(ProjectHubState& hub)
    {
        hub.m_discoveredProjects = DiscoverProjects(hub.m_engineRoot);
    }

    void InitializeProjectHub(ProjectHubState& hub)
    {
        hub.m_engineRoot = FindEngineRoot();
        hub.m_discoveredProjects = DiscoverProjects(hub.m_engineRoot);

        const std::filesystem::path defaultDirectory = GetDefaultProjectsDirectory();
        if (!defaultDirectory.empty())
        {
            CopyStringToBuffer(defaultDirectory.generic_string(), hub.m_createDirectory.data(),
                               hub.m_createDirectory.size());
        }

        CopyStringToBuffer("MyGame", hub.m_createName.data(), hub.m_createName.size());
        CopyStringToBuffer("0.1.0", hub.m_createVersion.data(), hub.m_createVersion.size());

        std::error_code ec;
        const std::filesystem::path currentProject = std::filesystem::current_path(ec) / "project.hive";
        if (!ec && std::filesystem::exists(currentProject))
        {
            CopyStringToBuffer(currentProject.generic_string(), hub.m_openPath.data(), hub.m_openPath.size());
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
    ScopedComApartment::ScopedComApartment()
        : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))
    {
    }

    ScopedComApartment::~ScopedComApartment()
    {
        if (SUCCEEDED(m_result))
        {
            CoUninitialize();
        }
    }

    bool ScopedComApartment::IsReady() const
    {
        return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
    }

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
#endif // HIVE_PLATFORM_WINDOWS

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
#endif // !HIVE_PLATFORM_WINDOWS

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
        const CLSID dialogClassId =
            mode == NativePathPickerMode::SCENE_FILE_SAVE ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
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
            kdialogCommand = "kdialog --getsavefilename " + QuoteShellArgument(seed) + " " + QuoteShellArgument(filter);
        }
        else
        {
            kdialogCommand = "kdialog --getopenfilename " + QuoteShellArgument(seed) + " " + QuoteShellArgument(filter);
        }
        if (RunShellDialogCommand(kdialogCommand, &selected))
        {
            *selectedPath = std::filesystem::path{selected};
            return NativePathPickerResult::SUCCESS;
        }

        return NativePathPickerResult::UNAVAILABLE;
#endif
    }

    [[maybe_unused]] std::string CompactPathForUi(const std::string& value, size_t maxChars)
    {
        if (value.size() <= maxChars || maxChars < 8)
        {
            return value;
        }

        const size_t tailChars = maxChars - 3;
        return "..." + value.substr(value.size() - tailChars);
    }

    [[maybe_unused]] bool PickPathIntoBuffer(ProjectHubState& hub, NativePathPickerMode mode,
                                             const std::filesystem::path& seedDirectory, char* buffer,
                                             size_t bufferSize)
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
#endif // HIVE_MODE_EDITOR

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

} // namespace brood::launcher
