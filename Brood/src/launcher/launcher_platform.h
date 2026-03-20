#pragma once

#include <hive/hive_config.h>

#include <brood/launcher/launcher_types.h>

#if HIVE_PLATFORM_WINDOWS
#include <Windows.h>
#include <shobjidl.h>
#endif

#include <wax/containers/string.h>
#include <wax/containers/vector.h>

#include <filesystem>

namespace brood::launcher
{

#if HIVE_MODE_EDITOR
    std::filesystem::path GetCurrentExecutablePath();
    bool IsEngineRoot(const std::filesystem::path& path);
    wax::String GetEnvironmentValue(const char* name);
    std::filesystem::path FindEngineRoot();
    std::filesystem::path GetHomeDirectory();
    std::filesystem::path GetDefaultProjectsDirectory();
    void CopyStringToBuffer(const wax::String& value, char* buffer, size_t bufferSize);
    wax::String TrimmedCopy(const char* input);
    bool IsProjectNameValid(const wax::String& name);
    bool TryLoadProjectSummary(const std::filesystem::path& path, wax::String* name, wax::String* version);
    const char* GetPresetBase(LauncherToolchainPreset preset);
    wax::Vector<LauncherDiscoveredProject> DiscoverProjects(const std::filesystem::path& engineRoot);
    void SetHubStatus(ProjectHubState& hub, bool isError, const wax::String& message);
    void RefreshProjectHub(ProjectHubState& hub);
    void InitializeProjectHub(ProjectHubState& hub);
    std::filesystem::path BuildCreateTargetPath(const ProjectHubState& hub, const wax::String& projectName);
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
    wax::String QuoteShellArgument(const wax::String& value);
    bool RunShellDialogCommand(const wax::String& command, wax::String* output);
#endif

    NativePathPickerResult ShowNativePathPicker(NativePathPickerMode mode, const std::filesystem::path& seedPath,
                                                std::filesystem::path* selectedPath);
    [[maybe_unused]] wax::String CompactPathForUi(const wax::String& value, size_t maxChars);
    [[maybe_unused]] bool PickPathIntoBuffer(ProjectHubState& hub, NativePathPickerMode mode,
                                             const std::filesystem::path& seedDirectory, char* buffer,
                                             size_t bufferSize);
#endif // HIVE_MODE_EDITOR

    bool ResolveProjectFilePath(const std::filesystem::path& inputPath, std::filesystem::path* resolvedPath);
    wax::String BuildWindowTitle(const wax::String& projectPath);

} // namespace brood::launcher
