#pragma once

#include <hive/HiveConfig.h>

#include <brood/launcher/launcher_types.h>

#if HIVE_PLATFORM_WINDOWS
#include <Windows.h>
#include <shobjidl.h>
#endif

#include <filesystem>
#include <string>
#include <vector>

namespace brood::launcher
{

#if HIVE_MODE_EDITOR
    std::filesystem::path GetCurrentExecutablePath();
    bool IsEngineRoot(const std::filesystem::path& path);
    std::string GetEnvironmentValue(const char* name);
    std::filesystem::path FindEngineRoot();
    std::filesystem::path GetHomeDirectory();
    std::filesystem::path GetDefaultProjectsDirectory();
    void CopyStringToBuffer(const std::string& value, char* buffer, size_t bufferSize);
    std::string TrimmedCopy(const char* input);
    bool IsProjectNameValid(const std::string& name);
    bool TryLoadProjectSummary(const std::filesystem::path& path, std::string* name, std::string* version);
    const char* GetPresetBase(LauncherToolchainPreset preset);
    std::vector<LauncherDiscoveredProject> DiscoverProjects(const std::filesystem::path& engineRoot);
    void SetHubStatus(ProjectHubState& hub, bool isError, const std::string& message);
    void RefreshProjectHub(ProjectHubState& hub);
    void InitializeProjectHub(ProjectHubState& hub);
    std::filesystem::path BuildCreateTargetPath(const ProjectHubState& hub, const std::string& projectName);
    [[maybe_unused]] std::filesystem::path GetPickerSeedDirectory(const char* rawPath);

#if HIVE_PLATFORM_WINDOWS
    class ScopedComApartment
    {
    public:
        ScopedComApartment();
        ~ScopedComApartment();
        bool IsReady() const;

    private:
        HRESULT m_result;
    };

    template <typename T> class ScopedComPtr
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

    void SetNativeDialogInitialPath(IFileDialog* dialog, const std::filesystem::path& seedPath);
#endif

#if !HIVE_PLATFORM_WINDOWS
    std::string QuoteShellArgument(const std::string& value);
    bool RunShellDialogCommand(const std::string& command, std::string* output);
#endif

    NativePathPickerResult ShowNativePathPicker(NativePathPickerMode mode, const std::filesystem::path& seedPath,
                                                std::filesystem::path* selectedPath);
    [[maybe_unused]] std::string CompactPathForUi(const std::string& value, size_t maxChars);
    [[maybe_unused]] bool PickPathIntoBuffer(ProjectHubState& hub, NativePathPickerMode mode,
                                             const std::filesystem::path& seedDirectory, char* buffer,
                                             size_t bufferSize);
#endif // HIVE_MODE_EDITOR

    bool ResolveProjectFilePath(const std::filesystem::path& inputPath, std::filesystem::path* resolvedPath);
    std::string BuildWindowTitle(const std::string& projectPath);

} // namespace brood::launcher
