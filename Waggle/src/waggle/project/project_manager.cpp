#include <hive/core/log.h>

#include <wax/containers/string.h>
#include <wax/serialization/binary_reader.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/byte_buffer.h>

#include <nectar/assets/material_asset.h>
#include <nectar/assets/mesh_asset.h>
#include <nectar/assets/texture_asset.h>
#include <nectar/cas/cas_store.h>
#include <nectar/database/dep_cache.h>
#include <nectar/core/asset_id.h>
#include <nectar/database/asset_database.h>
#include <nectar/database/asset_record.h>
#include <nectar/io/io_scheduler.h>
#include <nectar/pipeline/cook_cache.h>
#include <nectar/pipeline/cook_cache_persist.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/cooker_registry.h>
#include <nectar/pipeline/hot_reload.h>
#include <nectar/pipeline/i_asset_cooker.h>
#include <nectar/pipeline/i_asset_importer.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/registry/hiveid_file.h>
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
    constexpr uint16_t kImportCacheVersion = 2;

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
        if (!reader.TryRead(version) || (version != kImportCacheVersion && version != 1))
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

            wax::ByteSpan importSourceSpan{};
            if (version >= 2)
                importSourceSpan = reader.ReadString();

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
            record.m_importSource = wax::String{alloc};
            if (importSourceSpan.Size() > 0)
                record.m_importSource.Append(reinterpret_cast<const char*>(importSourceSpan.Data()),
                                             importSourceSpan.Size());
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
            writer.WriteString(r.m_importSource.CStr(), r.m_importSource.Size());
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

        template <typename T> void SwapAsset(nectar::AssetServer& server, wax::StringView path, wax::ByteSpan data)
        {
            auto handle = server.FindHandle<T>(path);
            if (!handle.IsNull())
                server.Reload<T>(handle, data);
        }
    } // namespace

    ProjectManager::ProjectManager(comb::DefaultAllocator& alloc, drone::JobSubmitter jobs)
        : m_alloc{&alloc}
        , m_jobs{jobs}
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

        std::filesystem::path fsPath{wax::String{projectHivePath}.CStr()};
        auto rootStr = fsPath.parent_path().generic_string();
        wax::StringView rootView{rootStr.c_str(), rootStr.size()};

        m_paths = m_project.ResolvePaths(rootView);

        std::error_code ec;
        std::filesystem::create_directories(m_paths.m_cache.CStr(), ec);
        std::filesystem::create_directories(m_paths.m_cas.CStr(), ec);

        m_vfs = wax::MakeBox<nectar::VirtualFilesystem>(*m_alloc, *m_alloc);
        m_assetsMount = wax::MakeBox<nectar::DiskMountSource>(*m_alloc, m_paths.m_assets, *m_alloc);
        m_vfs->Mount("", m_assetsMount.Get());
        m_casMount = wax::MakeBox<nectar::DiskMountSource>(*m_alloc, m_paths.m_cas, *m_alloc);
        m_vfs->Mount("cas", m_casMount.Get());

        m_cas = wax::MakeBox<nectar::CasStore>(*m_alloc, *m_alloc, m_paths.m_cas);
        m_io = wax::MakeBox<nectar::IOScheduler>(*m_alloc, *m_vfs, *m_alloc, m_jobs);
        m_server = wax::MakeBox<nectar::AssetServer>(*m_alloc, *m_alloc, *m_vfs, *m_io);

        m_importerRegistry = wax::MakeBox<nectar::ImporterRegistry>(*m_alloc, *m_alloc);
        m_cookerRegistry = wax::MakeBox<nectar::CookerRegistry>(*m_alloc, *m_alloc);

        m_importDb = wax::MakeBox<nectar::AssetDatabase>(*m_alloc, *m_alloc);
        LoadImportCache(m_paths.m_importCache.CStr(), *m_importDb, *m_alloc);

        {
            wax::String depPath{*m_alloc};
            depPath.Append(m_paths.m_cache.Data(), m_paths.m_cache.Size());
            depPath.Append("/dep_graph.bin");
            nectar::LoadDependencyCache(depPath.View(), m_importDb->GetDependencyGraph(), *m_alloc);
        }

        // Scan .hiveid sidecar files — only for assets not already in the import cache.
        // When a watcher state file exists, DetectOfflineChanges will handle
        // CREATED/MODIFIED/DELETED efficiently. This scan is the fallback for
        // first-run or missing watcher state.
        {
            std::error_code scanEc;
            std::filesystem::path assetsPath{m_paths.m_assets.CStr()};
            if (std::filesystem::exists(assetsPath, scanEc))
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath, scanEc))
                {
                    if (!entry.is_regular_file())
                        continue;

                    auto ext = entry.path().extension().string();
                    if (ext != ".hiveid")
                        continue;

                    auto assetFilePath = entry.path();
                    assetFilePath.replace_extension();

                    auto assetRelative = std::filesystem::relative(assetFilePath, assetsPath, scanEc).generic_string();

                    nectar::HiveIdData hid{};
                    if (!nectar::ReadHiveId(assetFilePath.string().c_str(), hid, *m_alloc))
                        continue;

                    if (m_importDb->Contains(hid.m_guid))
                        continue;

                    nectar::AssetRecord rec{};
                    rec.m_uuid = hid.m_guid;
                    rec.m_path = wax::String{*m_alloc, wax::StringView{assetRelative.c_str()}};
                    rec.m_type = wax::String{*m_alloc, hid.m_type.View()};
                    rec.m_name = wax::String{*m_alloc, wax::StringView{assetFilePath.stem().string().c_str()}};
                    rec.m_labels = wax::Vector<wax::String>{*m_alloc};
                    m_importDb->Insert(static_cast<nectar::AssetRecord&&>(rec));
                }
            }
        }

        m_importPipeline =
            wax::MakeBox<nectar::ImportPipeline>(*m_alloc, *m_alloc, *m_importerRegistry, *m_cas, *m_vfs, *m_importDb);
        m_cookCache = wax::MakeBox<nectar::CookCache>(*m_alloc, *m_alloc);
        {
            wax::String cookPath{*m_alloc};
            cookPath.Append(m_paths.m_cache.Data(), m_paths.m_cache.Size());
            cookPath.Append("/cook_cache.bin");
            nectar::LoadCookCache(cookPath.View(), *m_cookCache, *m_alloc);
        }
        m_cookPipeline = wax::MakeBox<nectar::CookPipeline>(*m_alloc, *m_alloc, *m_cookerRegistry, *m_cas, *m_importDb,
                                                            *m_cookCache, m_jobs);

        if (config.m_enableHotReload)
        {
            m_nativeWatcher = wax::MakeBox<nectar::NativeFileWatcher>(*m_alloc, *m_alloc);
            m_watcher = m_nativeWatcher.Get();
            m_hotReload = wax::MakeBox<nectar::HotReloadManager>(*m_alloc, *m_alloc, *m_watcher, *m_importDb,
                                                                 *m_importPipeline, *m_cookPipeline, m_vfs.Get());
            m_hotReload->SetBaseDirectory(m_paths.m_assets.View());
            m_hotReload->WatchDirectory(m_paths.m_assets.View());
            m_hotReload->SetImportSettingsProvider(
                [](nectar::AssetId, wax::StringView vfsPath, nectar::HiveDocument& settings, void* ud) {
                    auto* self = static_cast<ProjectManager*>(ud);
                    wax::String pathStr{vfsPath};
                    auto parentDir = std::filesystem::path{pathStr.CStr()}.parent_path().generic_string();
                    if (!parentDir.empty())
                    {
                        wax::String fullDir{self->m_paths.m_assets.View()};
                        fullDir.Append("/");
                        fullDir.Append(parentDir.c_str(), parentDir.size());
                        fullDir.Append("/");
                        settings.SetValue("import", "base_path",
                                          nectar::HiveValue::MakeString(*self->m_alloc, fullDir.View()));

                        wax::String vfsDir{*self->m_alloc};
                        vfsDir.Append(parentDir.c_str(), parentDir.size());
                        vfsDir.Append("/", 1);
                        settings.SetValue("import", "vfs_dir",
                                          nectar::HiveValue::MakeString(*self->m_alloc, vfsDir.View()));
                    }
                },
                this);

            wax::String statePath{*m_alloc};
            statePath.Append(m_paths.m_cache.Data(), m_paths.m_cache.Size());
            statePath.Append("/watcher_state.bin", 18);
            m_nativeWatcher->LoadState(statePath.View());
            m_nativeWatcher->DetectOfflineChanges(m_offlineChanges);
            m_watcherStatePath = static_cast<wax::String&&>(statePath);

            // Incremental startup: process offline changes immediately
            if (m_offlineChanges.Size() > 0)
            {
                std::filesystem::path assetsPath{m_paths.m_assets.CStr()};
                wax::Vector<nectar::AssetId> toReimport{*m_alloc};

                for (size_t i = 0; i < m_offlineChanges.Size(); ++i)
                {
                    auto& change = m_offlineChanges[i];
                    wax::StringView absPath = change.m_path.View();

                    // Skip .hiveid sidecar files
                    if (absPath.Size() > 7)
                    {
                        wax::StringView suffix{absPath.Data() + absPath.Size() - 7, 7};
                        if (suffix == wax::StringView{".hiveid", 7})
                            continue;
                    }

                    // Convert absolute path to VFS relative path
                    wax::String vfsPath{*m_alloc};
                    if (absPath.Size() > m_paths.m_assets.Size() + 1)
                    {
                        size_t prefixLen = m_paths.m_assets.Size();
                        if (absPath[prefixLen] == '/')
                            ++prefixLen;
                        vfsPath.Append(absPath.Data() + prefixLen, absPath.Size() - prefixLen);
                    }
                    else
                    {
                        vfsPath.Append(absPath.Data(), absPath.Size());
                    }

                    if (change.m_kind == nectar::FileChangeKind::MODIFIED)
                    {
                        auto* record = m_importDb->FindByPath(vfsPath.View());
                        if (record)
                            toReimport.PushBack(record->m_uuid);
                    }
                    else if (change.m_kind == nectar::FileChangeKind::DELETED)
                    {
                        auto* record = m_importDb->FindByPath(vfsPath.View());
                        if (record)
                        {
                            m_importDb->GetDependencyGraph().RemoveNode(record->m_uuid);
                            m_cookCache->Invalidate(record->m_uuid);
                        }
                    }
                }

                // Version check: add assets whose importer version bumped
                m_importDb->ForEach([&](nectar::AssetId id, const nectar::AssetRecord& rec) {
                    nectar::IAssetImporter* imp = m_importerRegistry->FindByPath(rec.m_path.View());
                    if (imp && rec.m_importVersion != imp->Version())
                        toReimport.PushBack(id);
                });

                if (toReimport.Size() > 0)
                {
                    size_t reimported = m_importPipeline->ReimportOutdated(toReimport);
                    hive::LogInfo(LOG_PROJECT, "Incremental startup: {}/{} assets reimported", reimported,
                                  toReimport.Size());

                    for (size_t i = 0; i < toReimport.Size(); ++i)
                        m_cookPipeline->InvalidateCascade(toReimport[i]);

                    nectar::CookRequest cookReq;
                    cookReq.m_assets = static_cast<wax::Vector<nectar::AssetId>&&>(toReimport);
                    cookReq.m_platform = kDefaultPlatform;
                    cookReq.m_workerCount = 1;
                    auto cookResult = m_cookPipeline->CookAll(cookReq);
                    hive::LogInfo(LOG_PROJECT, "Incremental startup: {} cooked, {} skipped, {} failed",
                                  cookResult.m_cooked, cookResult.m_skipped, cookResult.m_failed);
                }

                m_offlineChanges.Clear();
            }

            m_serviceThread.Register(m_nativeWatcher.Get());
            m_serviceThread.Start();
        }

        m_open = true;
        hive::LogInfo(LOG_PROJECT, "Project '{}' opened (root: {})", m_project.Name().Data(), m_paths.m_root.CStr());
        return true;
    }

    void ProjectManager::Close()
    {
        if (!m_open)
            return;

        if (m_io)
            m_io->Shutdown();

        SaveImportCache();

        if (m_importDb)
        {
            wax::String depPath{*m_alloc};
            depPath.Append(m_paths.m_cache.Data(), m_paths.m_cache.Size());
            depPath.Append("/dep_graph.bin");
            nectar::SaveDependencyCache(depPath.View(), m_importDb->GetDependencyGraph(), *m_alloc);
        }

        if (m_cookCache)
        {
            wax::String cookPath{*m_alloc};
            cookPath.Append(m_paths.m_cache.Data(), m_paths.m_cache.Size());
            cookPath.Append("/cook_cache.bin");
            nectar::SaveCookCache(cookPath.View(), *m_cookCache, *m_alloc);
        }

        m_serviceThread.Stop();

        if (m_nativeWatcher && m_watcherStatePath.Size() > 0)
            m_nativeWatcher->SaveState(m_watcherStatePath.View());

        m_hotReload.Reset();
        m_nativeWatcher.Reset();
        m_watcher = nullptr;
        m_cookPipeline.Reset();
        m_cookCache.Reset();
        m_importPipeline.Reset();
        m_importDb.Reset();
        m_cookerRegistry.Reset();
        m_importerRegistry.Reset();
        m_server.Reset();
        m_io.Reset();
        m_cas.Reset();
        m_casMount.Reset();
        m_assetsMount.Reset();
        m_vfs.Reset();

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
        return m_hotReload.Get();
    }

    nectar::IFileWatcher* ProjectManager::Watcher() noexcept
    {
        return m_watcher;
    }

    void ProjectManager::SaveImportCache()
    {
        if (m_importDb.IsNull() || !m_open)
            return;

        if (SaveImportCacheToDisk(m_paths.m_importCache.CStr(), *m_importDb, *m_alloc))
            hive::LogInfo(LOG_PROJECT, "Import cache saved");
        else
            hive::LogWarning(LOG_PROJECT, "Failed to save import cache");
    }

    void ProjectManager::Update()
    {
        if (m_server)
            m_server->Update();

        if (m_hotReload)
        {
            if (m_offlineChanges.Size() > 0 && m_nativeWatcher)
            {
                for (size_t i = 0; i < m_offlineChanges.Size(); ++i)
                    m_nativeWatcher->EnqueueChange(m_offlineChanges[i].m_path.View(), m_offlineChanges[i].m_kind);
                m_offlineChanges.Clear();
            }
            m_lastReloadCount = static_cast<uint32_t>(m_hotReload->ProcessChanges(kDefaultPlatform));

            if (m_lastReloadCount > 0 && m_server)
            {
                const auto& reloaded = m_hotReload->LastReloaded();
                for (size_t i = 0; i < reloaded.Size(); ++i)
                {
                    auto* record = m_importDb->FindByUuid(reloaded[i]);
                    if (!record)
                        continue;

                    const auto* cacheEntry = m_cookCache->Find(reloaded[i], kDefaultPlatform);
                    if (!cacheEntry)
                        continue;

                    auto cookedBlob = m_cas->Load(cacheEntry->m_cookedHash);
                    if (cookedBlob.Size() == 0)
                        continue;

                    auto blobSpan = cookedBlob.View();
                    if (record->m_type.View().Equals(wax::StringView{"Texture", 7}))
                        SwapAsset<nectar::TextureAsset>(*m_server, record->m_path.View(), blobSpan);
                    else if (record->m_type.View().Equals(wax::StringView{"Mesh", 4}))
                        SwapAsset<nectar::MeshAsset>(*m_server, record->m_path.View(), blobSpan);
                    else if (record->m_type.View().Equals(wax::StringView{"Material", 8}))
                        SwapAsset<nectar::MaterialAsset>(*m_server, record->m_path.View(), blobSpan);

                    hive::LogInfo(LOG_PROJECT, "Swapped asset: {} ({})", record->m_path.CStr(), record->m_type.CStr());
                }
            }
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
