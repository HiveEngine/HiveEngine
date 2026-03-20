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

    wax::String GetEnvironmentValue(const char* name)
    {
#if HIVE_PLATFORM_WINDOWS
        char* value = nullptr;
        size_t valueLength = 0;
        if (_dupenv_s(&value, &valueLength, name) != 0 || value == nullptr)
        {
            return {};
        }

        wax::String result{value};
        std::free(value);
        return result;
#else
        const char* value = std::getenv(name);
        return value != nullptr ? wax::String{value} : wax::String{};
#endif
    }

    std::filesystem::path FindEngineRoot()
    {
        const wax::String enginePath = GetEnvironmentValue("HIVE_ENGINE_DIR");
        if (!enginePath.IsEmpty())
        {
            const std::filesystem::path root{enginePath.CStr()};
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
        const wax::String userProfile = GetEnvironmentValue("USERPROFILE");
        if (!userProfile.IsEmpty())
        {
            return std::filesystem::path{userProfile.CStr()};
        }

        const wax::String homeDrive = GetEnvironmentValue("HOMEDRIVE");
        const wax::String homePath = GetEnvironmentValue("HOMEPATH");
        if (!homeDrive.IsEmpty() && !homePath.IsEmpty())
        {
            return std::filesystem::path{(homeDrive + homePath).CStr()};
        }
#else
        const wax::String home = GetEnvironmentValue("HOME");
        if (!home.IsEmpty())
        {
            return std::filesystem::path{home.CStr()};
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

    void CopyStringToBuffer(const wax::String& value, char* buffer, size_t bufferSize)
    {
        if (bufferSize == 0)
        {
            return;
        }

        const size_t copyLength = (std::min)(bufferSize - 1, value.Size());
        if (copyLength > 0)
        {
            std::memcpy(buffer, value.Data(), copyLength);
        }
        buffer[copyLength] = '\0';
    }

    wax::String TrimmedCopy(const char* input)
    {
        if (input == nullptr)
        {
            return {};
        }

        wax::StringView view{input};
        size_t first = 0;
        while (first < view.Size() &&
               (view[first] == ' ' || view[first] == '\t' || view[first] == '\r' || view[first] == '\n'))
        {
            ++first;
        }

        if (first == view.Size())
        {
            return {};
        }

        size_t last = view.Size() - 1;
        while (last > first && (view[last] == ' ' || view[last] == '\t' || view[last] == '\r' || view[last] == '\n'))
        {
            --last;
        }

        return wax::String{view.Substr(first, last - first + 1)};
    }

    bool IsProjectNameValid(const wax::String& name)
    {
        if (name.IsEmpty())
        {
            return false;
        }

        for (const char ch : name.View())
        {
            const bool isAlphaNum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
            if (!isAlphaNum && ch != '_' && ch != '-')
            {
                return false;
            }
        }

        return true;
    }

    bool TryLoadProjectSummary(const std::filesystem::path& path, wax::String* name, wax::String* version)
    {
        comb::ModuleAllocator tmpAlloc{"TmpProjectSummary", size_t{4} * 1024 * 1024};
        nectar::ProjectFile projectFile{tmpAlloc.Get()};
        const wax::String projectPath{path.generic_string().c_str()};
        auto loadResult = projectFile.LoadFromDisk({projectPath.CStr(), projectPath.Size()});
        if (!loadResult.m_success)
        {
            return false;
        }

        *name = wax::String{projectFile.Name()};
        *version = wax::String{projectFile.Version()};
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

    wax::Vector<LauncherDiscoveredProject> DiscoverProjects(const std::filesystem::path& engineRoot)
    {
        wax::Vector<LauncherDiscoveredProject> projects;

        std::error_code ec;
        const std::filesystem::path currentProject = std::filesystem::current_path(ec) / "project.hive";
        if (!ec && std::filesystem::exists(currentProject))
        {
            LauncherDiscoveredProject summary{};
            summary.m_path = wax::String{currentProject.generic_string().c_str()};
            summary.m_isCurrentDirectory = true;
            if (!TryLoadProjectSummary(currentProject, &summary.m_name, &summary.m_version))
            {
                summary.m_name = wax::String{currentProject.parent_path().filename().string().c_str()};
                summary.m_version = "Unknown version";
            }
            projects.PushBack(std::move(summary));
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
                summary.m_path = wax::String{projectFile.generic_string().c_str()};
                if (!TryLoadProjectSummary(projectFile, &summary.m_name, &summary.m_version))
                {
                    summary.m_name = wax::String{it->path().filename().string().c_str()};
                    summary.m_version = "Unknown version";
                }
                projects.PushBack(std::move(summary));
            }
        }

        std::sort(projects.Begin(), projects.End(),
                  [](const LauncherDiscoveredProject& lhs, const LauncherDiscoveredProject& rhs) {
                      if (lhs.m_isCurrentDirectory != rhs.m_isCurrentDirectory)
                      {
                          return lhs.m_isCurrentDirectory;
                      }
                      return lhs.m_name < rhs.m_name;
                  });

        return projects;
    }

    void SetHubStatus(ProjectHubState& hub, bool isError, const wax::String& message)
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
            CopyStringToBuffer(wax::String{defaultDirectory.generic_string().c_str()}, hub.m_createDirectory.Data(),
                               hub.m_createDirectory.Size());
        }

        CopyStringToBuffer("MyGame", hub.m_createName.Data(), hub.m_createName.Size());
        CopyStringToBuffer("0.1.0", hub.m_createVersion.Data(), hub.m_createVersion.Size());

        std::error_code ec;
        const std::filesystem::path currentProject = std::filesystem::current_path(ec) / "project.hive";
        if (!ec && std::filesystem::exists(currentProject))
        {
            CopyStringToBuffer(wax::String{currentProject.generic_string().c_str()}, hub.m_openPath.Data(),
                               hub.m_openPath.Size());
        }
    }

    std::filesystem::path BuildCreateTargetPath(const ProjectHubState& hub, const wax::String& projectName)
    {
        const wax::String createDirectory = TrimmedCopy(hub.m_createDirectory.Data());
        if (createDirectory.IsEmpty() || projectName.IsEmpty())
        {
            return {};
        }

        return std::filesystem::path{createDirectory.CStr()} / projectName.CStr();
    }

    [[maybe_unused]] std::filesystem::path GetPickerSeedDirectory(const char* rawPath)
    {
        const wax::String value = TrimmedCopy(rawPath);
        if (value.IsEmpty())
        {
            return {};
        }

        std::error_code ec;
        const std::filesystem::path candidate{value.CStr()};
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
    wax::String QuoteShellArgument(const wax::String& value)
    {
        wax::String escaped{"'"};
        for (size_t i = 0; i < value.Size(); ++i)
        {
            const char ch = value[i];
            if (ch == '\'')
            {
                escaped.Append("'\"'\"'");
            }
            else
            {
                escaped.Append(ch);
            }
        }
        escaped.Append('\'');
        return escaped;
    }

    bool RunShellDialogCommand(const wax::String& command, wax::String* output)
    {
        FILE* pipe = popen(command.CStr(), "r");
        if (pipe == nullptr)
        {
            return false;
        }

        wax::String result;
        char buffer[512];
        while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr)
        {
            result.Append(buffer);
        }

        const int status = pclose(pipe);
        size_t end = result.Size();
        while (end > 0 && (result[end - 1] == '\n' || result[end - 1] == '\r'))
        {
            --end;
        }
        result = wax::String{result.View().Substr(0, end)};

        if (status != 0 || result.IsEmpty())
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
        wax::String title = "Select Hive Project";
        wax::String filter = "*.hive|Hive Project";
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

        const wax::String seed{(seedPath.empty() ? GetHomeDirectory() : seedPath).generic_string().c_str()};

        wax::String selected{};
        wax::String zenityCommand = "zenity --file-selection ";
        if (mode == NativePathPickerMode::DIRECTORY)
        {
            zenityCommand.Append("--directory ");
        }
        else if (mode == NativePathPickerMode::SCENE_FILE_SAVE)
        {
            zenityCommand.Append("--save --confirm-overwrite ");
        }
        zenityCommand.Append("--title=" + QuoteShellArgument(title) + " ");
        if (!seed.IsEmpty())
        {
            zenityCommand.Append("--filename=" + QuoteShellArgument(seed) + " ");
        }
        if (mode == NativePathPickerMode::PROJECT_FILE)
        {
            zenityCommand.Append("--file-filter=" + QuoteShellArgument("Hive Project | project.hive *.hive") + " ");
        }
        else if (mode == NativePathPickerMode::SCENE_FILE_OPEN || mode == NativePathPickerMode::SCENE_FILE_SAVE)
        {
            zenityCommand.Append("--file-filter=" + QuoteShellArgument("Hive Scene | *.hscene") + " ");
        }

        if (RunShellDialogCommand(zenityCommand, &selected))
        {
            *selectedPath = std::filesystem::path{selected.CStr()};
            return NativePathPickerResult::SUCCESS;
        }

        wax::String kdialogCommand;
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
            *selectedPath = std::filesystem::path{selected.CStr()};
            return NativePathPickerResult::SUCCESS;
        }

        return NativePathPickerResult::UNAVAILABLE;
#endif
    }

    [[maybe_unused]] wax::String CompactPathForUi(const wax::String& value, size_t maxChars)
    {
        if (value.Size() <= maxChars || maxChars < 8)
        {
            return value;
        }

        const size_t tailChars = maxChars - 3;
        return wax::String{"..."} + wax::String{value.View().Substr(value.Size() - tailChars)};
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
                CopyStringToBuffer(wax::String{selectedPath.generic_string().c_str()}, buffer, bufferSize);
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

    wax::String BuildWindowTitle(const wax::String& projectPath)
    {
        wax::String windowTitle = "HiveEngine";
        comb::ModuleAllocator tmpAlloc{"TmpProjectParse", size_t{4} * 1024 * 1024};
        nectar::ProjectFile projectFile{tmpAlloc.Get()};
        auto loadResult = projectFile.LoadFromDisk({projectPath.CStr(), projectPath.Size()});
        if (loadResult.m_success && projectFile.Name().Size() > 0)
        {
            windowTitle.Append(" - ");
            windowTitle.Append(projectFile.Name().Data(), projectFile.Name().Size());
        }
        else
        {
            windowTitle.Append(" - ");
            windowTitle.Append(std::filesystem::path{projectPath.CStr()}.parent_path().filename().string().c_str());
        }

        return windowTitle;
    }

} // namespace brood::launcher
