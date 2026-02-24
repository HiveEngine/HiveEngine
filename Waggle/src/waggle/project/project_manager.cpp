#include <waggle/project/project_manager.h>

#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/vfs/disk_mount.h>
#include <nectar/cas/cas_store.h>
#include <nectar/io/io_scheduler.h>
#include <nectar/server/asset_server.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/pipeline/cooker_registry.h>
#include <nectar/database/asset_database.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/cook_cache.h>
#include <nectar/watcher/file_watcher.h>
#include <nectar/pipeline/hot_reload.h>
#include <nectar/pipeline/i_asset_importer.h>
#include <nectar/pipeline/i_asset_cooker.h>
#include <nectar/core/asset_id.h>
#include <comb/new.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/binary_reader.h>
#include <wax/serialization/byte_buffer.h>
#include <hive/core/log.h>

#include <filesystem>
#include <fstream>

namespace
{

static const hive::LogCategory LogProject{"Waggle.ProjectManager"};

constexpr uint32_t kImportCacheMagic = 0x4244494E;
constexpr uint16_t kImportCacheVersion = 1;

bool LoadImportCache(const char* path, nectar::AssetDatabase& db,
                     comb::DefaultAllocator& alloc)
{
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) return false;

    auto file_size = file.tellg();
    if (file_size < 12) return false;

    file.seekg(0);
    wax::ByteBuffer<> buf{alloc, static_cast<size_t>(file_size)};
    buf.Resize(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(buf.Data()), file_size);
    if (!file) return false;

    wax::BinaryReader reader{buf.View()};

    uint32_t magic{};
    if (!reader.TryRead(magic) || magic != kImportCacheMagic) return false;

    uint16_t version{};
    if (!reader.TryRead(version) || version != kImportCacheVersion) return false;

    reader.Skip(2);

    uint32_t count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < count; ++i)
    {
        if (reader.Remaining() < 16) break;

        uint64_t id_high = reader.Read<uint64_t>();
        uint64_t id_low = reader.Read<uint64_t>();

        auto path_span = reader.ReadString();
        auto type_span = reader.ReadString();
        auto name_span = reader.ReadString();

        uint64_t ch_high = reader.Read<uint64_t>();
        uint64_t ch_low = reader.Read<uint64_t>();
        uint64_t ih_high = reader.Read<uint64_t>();
        uint64_t ih_low = reader.Read<uint64_t>();

        uint32_t imp_version = reader.Read<uint32_t>();

        uint32_t label_count = reader.Read<uint32_t>();
        for (uint32_t j = 0; j < label_count; ++j)
            (void)reader.ReadString();

        nectar::AssetRecord record{};
        record.uuid = nectar::AssetId{id_high, id_low};
        record.path = wax::String<>{alloc};
        record.path.Append(reinterpret_cast<const char*>(path_span.Data()), path_span.Size());
        record.type = wax::String<>{alloc};
        record.type.Append(reinterpret_cast<const char*>(type_span.Data()), type_span.Size());
        record.name = wax::String<>{alloc};
        record.name.Append(reinterpret_cast<const char*>(name_span.Data()), name_span.Size());
        record.content_hash = nectar::ContentHash{ch_high, ch_low};
        record.intermediate_hash = nectar::ContentHash{ih_high, ih_low};
        record.import_version = imp_version;
        record.labels = wax::Vector<wax::String<>>{alloc};

        db.Insert(static_cast<nectar::AssetRecord&&>(record));
    }

    return true;
}

bool SaveImportCacheToDisk(const char* path, const nectar::AssetDatabase& db,
                           comb::DefaultAllocator& alloc)
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path{path}.parent_path(), ec);

    wax::BinaryWriter<comb::DefaultAllocator> writer{alloc, 4096};

    writer.Write<uint32_t>(kImportCacheMagic);
    writer.Write<uint16_t>(kImportCacheVersion);
    writer.Write<uint16_t>(0);
    writer.Write<uint32_t>(static_cast<uint32_t>(db.Count()));

    db.ForEach([&](nectar::AssetId id, const nectar::AssetRecord& r) {
        writer.Write<uint64_t>(id.High());
        writer.Write<uint64_t>(id.Low());
        writer.WriteString(r.path.CStr(), r.path.Size());
        writer.WriteString(r.type.CStr(), r.type.Size());
        writer.WriteString(r.name.CStr(), r.name.Size());
        writer.Write<uint64_t>(r.content_hash.High());
        writer.Write<uint64_t>(r.content_hash.Low());
        writer.Write<uint64_t>(r.intermediate_hash.High());
        writer.Write<uint64_t>(r.intermediate_hash.Low());
        writer.Write<uint32_t>(r.import_version);
        writer.Write<uint32_t>(0);
    });

    auto span = writer.View();
    std::ofstream file{path, std::ios::binary};
    if (!file) return false;

    file.write(reinterpret_cast<const char*>(span.Data()),
               static_cast<std::streamsize>(span.Size()));
    return file.good();
}

} // namespace

namespace waggle
{

ProjectManager::ProjectManager(comb::DefaultAllocator& alloc)
    : alloc_{&alloc}
    , project_{alloc}
{
}

ProjectManager::~ProjectManager()
{
    Close();
}

bool ProjectManager::Open(wax::StringView project_hive_path, const ProjectConfig& config)
{
    if (open_)
        Close();

    auto load_result = project_.LoadFromDisk(project_hive_path);
    if (!load_result.success)
    {
        hive::LogError(LogProject, "Failed to load project file");
        return false;
    }

    std::filesystem::path fs_path{std::string{project_hive_path.Data(), project_hive_path.Size()}};
    auto root_str = fs_path.parent_path().generic_string();
    wax::StringView root_view{root_str.c_str(), root_str.size()};

    paths_ = project_.ResolvePaths(root_view);

    std::error_code ec;
    std::filesystem::create_directories(std::string{paths_.cache.CStr(), paths_.cache.Size()}, ec);
    std::filesystem::create_directories(std::string{paths_.cas.CStr(), paths_.cas.Size()}, ec);

    vfs_ = comb::New<nectar::VirtualFilesystem>(*alloc_, *alloc_);
    assets_mount_ = comb::New<nectar::DiskMountSource>(*alloc_, paths_.assets, *alloc_);
    vfs_->Mount("", assets_mount_);
    cas_mount_ = comb::New<nectar::DiskMountSource>(*alloc_, paths_.cas, *alloc_);
    vfs_->Mount("cas", cas_mount_);

    cas_ = comb::New<nectar::CasStore>(*alloc_, *alloc_, paths_.cas);
    io_ = comb::New<nectar::IOScheduler>(*alloc_, *vfs_, *alloc_);
    server_ = comb::New<nectar::AssetServer>(*alloc_, *alloc_, *vfs_, *io_);

    importer_registry_ = comb::New<nectar::ImporterRegistry>(*alloc_, *alloc_);
    cooker_registry_ = comb::New<nectar::CookerRegistry>(*alloc_, *alloc_);

    import_db_ = comb::New<nectar::AssetDatabase>(*alloc_, *alloc_);
    LoadImportCache(paths_.import_cache.CStr(), *import_db_, *alloc_);

    import_pipeline_ = comb::New<nectar::ImportPipeline>(*alloc_, *alloc_, *importer_registry_, *cas_, *vfs_, *import_db_);
    cook_cache_ = comb::New<nectar::CookCache>(*alloc_, *alloc_);
    cook_pipeline_ = comb::New<nectar::CookPipeline>(*alloc_, *alloc_, *cooker_registry_, *cas_, *import_db_, *cook_cache_);

    if (config.enable_hot_reload)
    {
        watcher_ = comb::New<nectar::PollingFileWatcher>(*alloc_, *alloc_, config.watcher_interval_ms);
        hot_reload_ = comb::New<nectar::HotReloadManager>(*alloc_, *alloc_, *watcher_, *import_db_, *import_pipeline_, *cook_pipeline_);
    }

    open_ = true;
    hive::LogInfo(LogProject, "Project '{}' opened (root: {})",
                  project_.Name().Data(), paths_.root.CStr());
    return true;
}

void ProjectManager::Close()
{
    if (!open_)
        return;

    if (io_)
        io_->Shutdown();

    SaveImportCache();

    comb::Delete(*alloc_, hot_reload_);
    comb::Delete(*alloc_, watcher_);
    comb::Delete(*alloc_, cook_pipeline_);
    comb::Delete(*alloc_, cook_cache_);
    comb::Delete(*alloc_, import_pipeline_);
    comb::Delete(*alloc_, import_db_);
    comb::Delete(*alloc_, cooker_registry_);
    comb::Delete(*alloc_, importer_registry_);
    comb::Delete(*alloc_, server_);
    comb::Delete(*alloc_, io_);
    comb::Delete(*alloc_, cas_);
    comb::Delete(*alloc_, cas_mount_);
    comb::Delete(*alloc_, assets_mount_);
    comb::Delete(*alloc_, vfs_);

    hot_reload_ = nullptr;
    watcher_ = nullptr;
    cook_pipeline_ = nullptr;
    cook_cache_ = nullptr;
    import_pipeline_ = nullptr;
    import_db_ = nullptr;
    cooker_registry_ = nullptr;
    importer_registry_ = nullptr;
    server_ = nullptr;
    io_ = nullptr;
    cas_ = nullptr;
    cas_mount_ = nullptr;
    assets_mount_ = nullptr;
    vfs_ = nullptr;

    open_ = false;
}

bool ProjectManager::IsOpen() const noexcept
{
    return open_;
}

void ProjectManager::RegisterImporter(nectar::IAssetImporter* importer)
{
    importer_registry_->Register(importer);
}

void ProjectManager::RegisterCooker(nectar::IAssetCooker* cooker)
{
    cooker_registry_->Register(cooker);
}

const nectar::ProjectFile& ProjectManager::Project() const noexcept
{
    return project_;
}

const nectar::ProjectPaths& ProjectManager::Paths() const noexcept
{
    return paths_;
}

nectar::VirtualFilesystem& ProjectManager::VFS() noexcept
{
    return *vfs_;
}

nectar::AssetServer& ProjectManager::Server() noexcept
{
    return *server_;
}

nectar::ImportPipeline& ProjectManager::Import() noexcept
{
    return *import_pipeline_;
}

nectar::CookPipeline& ProjectManager::Cook() noexcept
{
    return *cook_pipeline_;
}

nectar::CookCache& ProjectManager::CookCacheRef() noexcept
{
    return *cook_cache_;
}

nectar::CasStore& ProjectManager::CAS() noexcept
{
    return *cas_;
}

nectar::AssetDatabase& ProjectManager::Database() noexcept
{
    return *import_db_;
}

nectar::IOScheduler& ProjectManager::IO() noexcept
{
    return *io_;
}

nectar::HotReloadManager* ProjectManager::HotReload() noexcept
{
    return hot_reload_;
}

nectar::PollingFileWatcher* ProjectManager::Watcher() noexcept
{
    return watcher_;
}

void ProjectManager::SaveImportCache()
{
    if (!import_db_ || !open_)
        return;

    if (SaveImportCacheToDisk(paths_.import_cache.CStr(), *import_db_, *alloc_))
        hive::LogInfo(LogProject, "Import cache saved");
    else
        hive::LogWarning(LogProject, "Failed to save import cache");
}

void ProjectManager::Update()
{
    if (server_)
        server_->Update();
}

} // namespace waggle
