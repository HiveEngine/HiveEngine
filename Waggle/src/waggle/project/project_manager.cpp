#include <hive/core/log.h>

#include <comb/new.h>

#include <wax/serialization/binary_reader.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/byte_buffer.h>

#include <nectar/cas/cas_store.h>
#include <nectar/core/asset_id.h>
#include <nectar/database/asset_database.h>
#include <nectar/io/io_scheduler.h>
#include <nectar/pipeline/cook_cache.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/cooker_registry.h>
#include <nectar/pipeline/hot_reload.h>
#include <nectar/pipeline/i_asset_cooker.h>
#include <nectar/pipeline/i_asset_importer.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/server/asset_server.h>
#include <nectar/vfs/disk_mount.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/watcher/file_watcher.h>

#include <waggle/project/project_manager.h>

#include <filesystem>
#include <fstream>

namespace
{

    static const hive::LogCategory LOG_PROJECT{"Waggle.ProjectManager"};

    constexpr uint32_t kImportCacheMagic = 0x4244494E;
    constexpr uint16_t kImportCacheVersion = 1;

    bool LoadImportCache(const char* path, nectar::AssetDatabase& db, comb::DefaultAllocator& alloc)
    {
        std::ifstream file{path, std::ios::binary | std::ios::ate};
        if (!file)
            return false;

        auto fileSize = file.tellg();
        if (fileSize < 12)
            return false;

        file.seekg(0);
        wax::ByteBuffer buf{alloc, static_cast<size_t>(fileSize)};
        buf.Resize(static_cast<size_t>(fileSize));
        file.read(reinterpret_cast<char*>(buf.Data()), fileSize);
        if (!file)
            return false;

        wax::BinaryReader reader{buf.View()};

        uint32_t magic{};
        if (!reader.TryRead(magic) || magic != kImportCacheMagic)
            return false;

        uint16_t version{};
        if (!reader.TryRead(version) || version != kImportCacheVersion)
            return false;

        reader.Skip(2);

        uint32_t count = reader.Read<uint32_t>();

        for (uint32_t i = 0; i < count; ++i)
        {
            if (reader.Remaining() < 16)
                break;

            uint64_t idHigh = reader.Read<uint64_t>();
            uint64_t idLow = reader.Read<uint64_t>();

            auto pathSpan = reader.ReadString();
            auto typeSpan = reader.ReadString();
            auto nameSpan = reader.ReadString();

            uint64_t chHigh = reader.Read<uint64_t>();
            uint64_t chLow = reader.Read<uint64_t>();
            uint64_t ihHigh = reader.Read<uint64_t>();
            uint64_t ihLow = reader.Read<uint64_t>();

            uint32_t impVersion = reader.Read<uint32_t>();

            uint32_t labelCount = reader.Read<uint32_t>();
            for (uint32_t j = 0; j < labelCount; ++j)
                (void)reader.ReadString();

            nectar::AssetRecord record{};
            record.m_uuid = nectar::AssetId{idHigh, idLow};
            record.m_path = wax::String{alloc};
            record.m_path.Append(reinterpret_cast<const char*>(pathSpan.Data()), pathSpan.Size());
            record.m_type = wax::String{alloc};
            record.m_type.Append(reinterpret_cast<const char*>(typeSpan.Data()), typeSpan.Size());
            record.m_name = wax::String{alloc};
            record.m_name.Append(reinterpret_cast<const char*>(nameSpan.Data()), nameSpan.Size());
            record.m_contentHash = nectar::ContentHash{chHigh, chLow};
            record.m_intermediateHash = nectar::ContentHash{ihHigh, ihLow};
            record.m_importVersion = impVersion;
            record.m_labels = wax::Vector<wax::String>{alloc};

            db.Insert(static_cast<nectar::AssetRecord&&>(record));
        }

        return true;
    }

    bool SaveImportCacheToDisk(const char* path, const nectar::AssetDatabase& db, comb::DefaultAllocator& alloc)
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path{path}.parent_path(), ec);

        wax::BinaryWriter writer{alloc, 4096};

        writer.Write<uint32_t>(kImportCacheMagic);
        writer.Write<uint16_t>(kImportCacheVersion);
        writer.Write<uint16_t>(0);
        writer.Write<uint32_t>(static_cast<uint32_t>(db.Count()));

        db.ForEach([&](nectar::AssetId id, const nectar::AssetRecord& r) {
            writer.Write<uint64_t>(id.High());
            writer.Write<uint64_t>(id.Low());
            writer.WriteString(r.m_path.CStr(), r.m_path.Size());
            writer.WriteString(r.m_type.CStr(), r.m_type.Size());
            writer.WriteString(r.m_name.CStr(), r.m_name.Size());
            writer.Write<uint64_t>(r.m_contentHash.High());
            writer.Write<uint64_t>(r.m_contentHash.Low());
            writer.Write<uint64_t>(r.m_intermediateHash.High());
            writer.Write<uint64_t>(r.m_intermediateHash.Low());
            writer.Write<uint32_t>(r.m_importVersion);
            writer.Write<uint32_t>(0);
        });

        auto span = writer.View();
        std::ofstream file{path, std::ios::binary};
        if (!file)
            return false;

        file.write(reinterpret_cast<const char*>(span.Data()), static_cast<std::streamsize>(span.Size()));
        return file.good();
    }

} // namespace

namespace waggle
{
    namespace
    {
        constexpr wax::StringView kDefaultPlatform{"pc", 2};
    } // namespace

    ProjectManager::ProjectManager(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_project{alloc}
        , m_offlineChanges{alloc}
        , m_watcherStatePath{alloc}
    {
    }

    ProjectManager::~ProjectManager()
    {
        Close();
    }

    bool ProjectManager::Open(wax::StringView projectHivePath, const ProjectConfig& config)
    {
        if (m_open)
            Close();

        auto loadResult = m_project.LoadFromDisk(projectHivePath);
        if (!loadResult.m_success)
        {
            hive::LogError(LOG_PROJECT, "Failed to load project file");
            return false;
        }

        std::filesystem::path fsPath{std::string{projectHivePath.Data(), projectHivePath.Size()}};
        auto rootStr = fsPath.parent_path().generic_string();
        wax::StringView rootView{rootStr.c_str(), rootStr.size()};

        m_paths = m_project.ResolvePaths(rootView);

        std::error_code ec;
        std::filesystem::create_directories(std::string{m_paths.m_cache.CStr(), m_paths.m_cache.Size()}, ec);
        std::filesystem::create_directories(std::string{m_paths.m_cas.CStr(), m_paths.m_cas.Size()}, ec);

        m_vfs = comb::New<nectar::VirtualFilesystem>(*m_alloc, *m_alloc);
        m_assetsMount = comb::New<nectar::DiskMountSource>(*m_alloc, m_paths.m_assets, *m_alloc);
        m_vfs->Mount("", m_assetsMount);
        m_casMount = comb::New<nectar::DiskMountSource>(*m_alloc, m_paths.m_cas, *m_alloc);
        m_vfs->Mount("cas", m_casMount);

        m_cas = comb::New<nectar::CasStore>(*m_alloc, *m_alloc, m_paths.m_cas);
        m_io = comb::New<nectar::IOScheduler>(*m_alloc, *m_vfs, *m_alloc);
        m_server = comb::New<nectar::AssetServer>(*m_alloc, *m_alloc, *m_vfs, *m_io);

        m_importerRegistry = comb::New<nectar::ImporterRegistry>(*m_alloc, *m_alloc);
        m_cookerRegistry = comb::New<nectar::CookerRegistry>(*m_alloc, *m_alloc);

        m_importDb = comb::New<nectar::AssetDatabase>(*m_alloc, *m_alloc);
        LoadImportCache(m_paths.m_importCache.CStr(), *m_importDb, *m_alloc);

        m_importPipeline =
            comb::New<nectar::ImportPipeline>(*m_alloc, *m_alloc, *m_importerRegistry, *m_cas, *m_vfs, *m_importDb);
        m_cookCache = comb::New<nectar::CookCache>(*m_alloc, *m_alloc);
        m_cookPipeline =
            comb::New<nectar::CookPipeline>(*m_alloc, *m_alloc, *m_cookerRegistry, *m_cas, *m_importDb, *m_cookCache);

        if (config.m_enableHotReload)
        {
            m_nativeWatcher = comb::New<nectar::NativeFileWatcher>(*m_alloc, *m_alloc);
            m_watcher = m_nativeWatcher;
            m_hotReload = comb::New<nectar::HotReloadManager>(*m_alloc, *m_alloc, *m_watcher, *m_importDb,
                                                              *m_importPipeline, *m_cookPipeline, m_vfs);
            m_hotReload->SetBaseDirectory(m_paths.m_assets.View());
            m_hotReload->WatchDirectory(m_paths.m_assets.View());
            m_hotReload->SetImportSettingsProvider(
                [](nectar::AssetId, wax::StringView vfsPath, nectar::HiveDocument& settings, void* ud) {
                    auto* self = static_cast<ProjectManager*>(ud);
                    std::string pathStr{vfsPath.Data(), vfsPath.Size()};
                    auto parentDir = std::filesystem::path{pathStr}.parent_path().generic_string();
                    if (!parentDir.empty())
                    {
                        std::string fullDir{self->m_paths.m_assets.Data(), self->m_paths.m_assets.Size()};
                        fullDir += "/" + parentDir + "/";
                        settings.SetValue(
                            "import", "base_path",
                            nectar::HiveValue::MakeString(*self->m_alloc, {fullDir.c_str(), fullDir.size()}));
                    }
                },
                this);

            wax::String statePath{*m_alloc};
            statePath.Append(m_paths.m_cache.Data(), m_paths.m_cache.Size());
            statePath.Append("/watcher_state.bin", 18);
            m_nativeWatcher->LoadState(statePath.View());
            m_nativeWatcher->DetectOfflineChanges(m_offlineChanges);
            m_watcherStatePath = static_cast<wax::String&&>(statePath);
        }

        m_open = true;
        hive::LogInfo(LOG_PROJECT, "Project '{}' opened (root: {})", m_project.Name().Data(), m_paths.m_root.CStr());
        return true;
    }

    void ProjectManager::Close()
    {
        if (!m_open)
            return;

        if (m_io != nullptr)
            m_io->Shutdown();

        SaveImportCache();

        if (m_nativeWatcher != nullptr && m_watcherStatePath.Size() > 0)
            m_nativeWatcher->SaveState(m_watcherStatePath.View());

        comb::Delete(*m_alloc, m_hotReload);
        comb::Delete(*m_alloc, m_watcher);
        comb::Delete(*m_alloc, m_cookPipeline);
        comb::Delete(*m_alloc, m_cookCache);
        comb::Delete(*m_alloc, m_importPipeline);
        comb::Delete(*m_alloc, m_importDb);
        comb::Delete(*m_alloc, m_cookerRegistry);
        comb::Delete(*m_alloc, m_importerRegistry);
        comb::Delete(*m_alloc, m_server);
        comb::Delete(*m_alloc, m_io);
        comb::Delete(*m_alloc, m_cas);
        comb::Delete(*m_alloc, m_casMount);
        comb::Delete(*m_alloc, m_assetsMount);
        comb::Delete(*m_alloc, m_vfs);

        m_hotReload = nullptr;
        m_nativeWatcher = nullptr;
        m_watcher = nullptr;
        m_cookPipeline = nullptr;
        m_cookCache = nullptr;
        m_importPipeline = nullptr;
        m_importDb = nullptr;
        m_cookerRegistry = nullptr;
        m_importerRegistry = nullptr;
        m_server = nullptr;
        m_io = nullptr;
        m_cas = nullptr;
        m_casMount = nullptr;
        m_assetsMount = nullptr;
        m_vfs = nullptr;

        m_open = false;
    }

    bool ProjectManager::IsOpen() const noexcept
    {
        return m_open;
    }

    void ProjectManager::RegisterImporter(nectar::IAssetImporter* importer)
    {
        m_importerRegistry->Register(importer);
    }

    void ProjectManager::RegisterCooker(nectar::IAssetCooker* cooker)
    {
        m_cookerRegistry->Register(cooker);
    }

    const nectar::ProjectFile& ProjectManager::Project() const noexcept
    {
        return m_project;
    }

    const nectar::ProjectPaths& ProjectManager::Paths() const noexcept
    {
        return m_paths;
    }

    nectar::VirtualFilesystem& ProjectManager::VFS() noexcept
    {
        return *m_vfs;
    }

    nectar::AssetServer& ProjectManager::Server() noexcept
    {
        return *m_server;
    }

    nectar::ImportPipeline& ProjectManager::Import() noexcept
    {
        return *m_importPipeline;
    }

    nectar::CookPipeline& ProjectManager::Cook() noexcept
    {
        return *m_cookPipeline;
    }

    nectar::CookCache& ProjectManager::CookCacheRef() noexcept
    {
        return *m_cookCache;
    }

    nectar::CasStore& ProjectManager::CAS() noexcept
    {
        return *m_cas;
    }

    nectar::AssetDatabase& ProjectManager::Database() noexcept
    {
        return *m_importDb;
    }

    nectar::IOScheduler& ProjectManager::IO() noexcept
    {
        return *m_io;
    }

    nectar::HotReloadManager* ProjectManager::HotReload() noexcept
    {
        return m_hotReload;
    }

    nectar::IFileWatcher* ProjectManager::Watcher() noexcept
    {
        return m_watcher;
    }

    void ProjectManager::SaveImportCache()
    {
        if (m_importDb == nullptr || !m_open)
            return;

        if (SaveImportCacheToDisk(m_paths.m_importCache.CStr(), *m_importDb, *m_alloc))
            hive::LogInfo(LOG_PROJECT, "Import cache saved");
        else
            hive::LogWarning(LOG_PROJECT, "Failed to save import cache");
    }

    void ProjectManager::Update()
    {
        if (m_server != nullptr)
            m_server->Update();

        if (m_hotReload != nullptr)
        {
            if (m_offlineChanges.Size() > 0 && m_nativeWatcher != nullptr)
            {
                for (size_t i = 0; i < m_offlineChanges.Size(); ++i)
                    m_nativeWatcher->EnqueueChange(m_offlineChanges[i].m_path.View(), m_offlineChanges[i].m_kind);
                m_offlineChanges.Clear();
            }
            m_lastReloadCount = static_cast<uint32_t>(m_hotReload->ProcessChanges(kDefaultPlatform));
        }
        else
        {
            m_lastReloadCount = 0;
        }
    }

    uint32_t ProjectManager::LastReloadCount() const noexcept
    {
        return m_lastReloadCount;
    }

} // namespace waggle
