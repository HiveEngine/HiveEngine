#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>

#include <nectar/project/project_file.h>
#include <nectar/watcher/file_watcher.h>

#include <cstdint>

namespace nectar
{
    class VirtualFilesystem;
    class DiskMountSource;
    class CasStore;
    class AssetDatabase;
    class ImporterRegistry;
    class ImportPipeline;
    class CookerRegistry;
    class CookPipeline;
    class CookCache;
    class AssetServer;
    class IOScheduler;
    class HotReloadManager;
    class IAssetImporter;
    class IAssetCooker;
} // namespace nectar

namespace waggle
{
    struct ProjectConfig
    {
        bool m_enableHotReload{false};
        uint32_t m_watcherIntervalMs{500};
    };

    class ProjectManager
    {
    public:
        explicit ProjectManager(comb::DefaultAllocator& alloc);
        ~ProjectManager();

        ProjectManager(const ProjectManager&) = delete;
        ProjectManager& operator=(const ProjectManager&) = delete;

        [[nodiscard]] bool Open(wax::StringView projectHivePath, const ProjectConfig& config = {});
        void Close();
        [[nodiscard]] bool IsOpen() const noexcept;

        void RegisterImporter(nectar::IAssetImporter* importer);
        void RegisterCooker(nectar::IAssetCooker* cooker);

        [[nodiscard]] const nectar::ProjectFile& Project() const noexcept;
        [[nodiscard]] const nectar::ProjectPaths& Paths() const noexcept;
        [[nodiscard]] nectar::VirtualFilesystem& VFS() noexcept;
        [[nodiscard]] nectar::AssetServer& Server() noexcept;
        [[nodiscard]] nectar::ImportPipeline& Import() noexcept;
        [[nodiscard]] nectar::CookPipeline& Cook() noexcept;
        [[nodiscard]] nectar::CookCache& CookCacheRef() noexcept;
        [[nodiscard]] nectar::CasStore& CAS() noexcept;
        [[nodiscard]] nectar::AssetDatabase& Database() noexcept;
        [[nodiscard]] nectar::IOScheduler& IO() noexcept;
        [[nodiscard]] nectar::HotReloadManager* HotReload() noexcept;
        [[nodiscard]] nectar::IFileWatcher* Watcher() noexcept;

        void SaveImportCache();
        void Update();

        [[nodiscard]] uint32_t LastReloadCount() const noexcept;

    private:
        comb::DefaultAllocator* m_alloc;
        bool m_open{false};

        nectar::ProjectFile m_project;
        nectar::ProjectPaths m_paths;

        nectar::VirtualFilesystem* m_vfs{};
        nectar::DiskMountSource* m_assetsMount{};
        nectar::DiskMountSource* m_casMount{};
        nectar::CasStore* m_cas{};
        nectar::IOScheduler* m_io{};
        nectar::AssetServer* m_server{};
        nectar::ImporterRegistry* m_importerRegistry{};
        nectar::AssetDatabase* m_importDb{};
        nectar::ImportPipeline* m_importPipeline{};
        nectar::CookerRegistry* m_cookerRegistry{};
        nectar::CookCache* m_cookCache{};
        nectar::CookPipeline* m_cookPipeline{};
        nectar::IFileWatcher* m_watcher{};
        nectar::NativeFileWatcher* m_nativeWatcher{};
        nectar::HotReloadManager* m_hotReload{};
        wax::Vector<nectar::FileChange> m_offlineChanges;
        wax::String m_watcherStatePath;
        uint32_t m_lastReloadCount{0};
    };
} // namespace waggle
