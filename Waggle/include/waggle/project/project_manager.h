#pragma once

#include <nectar/project/project_file.h>
#include <comb/default_allocator.h>
#include <wax/containers/string_view.h>
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
    class PollingFileWatcher;
    class HotReloadManager;
    class IAssetImporter;
    class IAssetCooker;
}

namespace waggle
{
    struct ProjectConfig
    {
        bool enable_hot_reload{false};
        uint32_t watcher_interval_ms{500};
    };

    class ProjectManager
    {
    public:
        explicit ProjectManager(comb::DefaultAllocator& alloc);
        ~ProjectManager();

        ProjectManager(const ProjectManager&) = delete;
        ProjectManager& operator=(const ProjectManager&) = delete;

        [[nodiscard]] bool Open(wax::StringView project_hive_path,
                                const ProjectConfig& config = {});
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
        [[nodiscard]] nectar::PollingFileWatcher* Watcher() noexcept;

        void SaveImportCache();
        void Update();

    private:
        comb::DefaultAllocator* alloc_;
        bool open_{false};

        nectar::ProjectFile project_;
        nectar::ProjectPaths paths_;

        nectar::VirtualFilesystem* vfs_{};
        nectar::DiskMountSource* assets_mount_{};
        nectar::DiskMountSource* cas_mount_{};
        nectar::CasStore* cas_{};
        nectar::IOScheduler* io_{};
        nectar::AssetServer* server_{};
        nectar::ImporterRegistry* importer_registry_{};
        nectar::AssetDatabase* import_db_{};
        nectar::ImportPipeline* import_pipeline_{};
        nectar::CookerRegistry* cooker_registry_{};
        nectar::CookCache* cook_cache_{};
        nectar::CookPipeline* cook_pipeline_{};
        nectar::PollingFileWatcher* watcher_{};
        nectar::HotReloadManager* hot_reload_{};
    };
}
