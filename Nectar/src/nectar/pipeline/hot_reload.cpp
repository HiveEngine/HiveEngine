#include <nectar/pipeline/hot_reload.h>

#include <hive/core/log.h>

#include <wax/containers/hash_map.h>

#include <nectar/core/content_hash.h>
#include <nectar/database/asset_database.h>
#include <nectar/database/asset_record.h>
#include <nectar/database/dependency_graph.h>
#include <nectar/registry/hiveid_file.h>
#include <nectar/hive/hive_document.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/watcher/file_watcher.h>

#include <filesystem>

namespace nectar
{
    static const hive::LogCategory LOG_HOTRELOAD{"Nectar.HotReload"};
    HotReloadManager::HotReloadManager(comb::DefaultAllocator& alloc, IFileWatcher& watcher, AssetDatabase& db,
                                       ImportPipeline& importPipe, CookPipeline& cookPipe, VirtualFilesystem* vfs)
        : m_alloc{&alloc}
        , m_watcher{&watcher}
        , m_db{&db}
        , m_importPipe{&importPipe}
        , m_cookPipe{&cookPipe}
        , m_vfs{vfs}
        , m_lastReloaded{alloc}
        , m_baseDir{alloc}
    {
    }

    void HotReloadManager::WatchDirectory(wax::StringView dir)
    {
        m_watcher->Watch(dir);
    }

    void HotReloadManager::SetBaseDirectory(wax::StringView baseDir)
    {
        m_baseDir = wax::String{*m_alloc};
        m_baseDir.Append(baseDir.Data(), baseDir.Size());

        for (size_t i = 0; i < m_baseDir.Size(); ++i)
        {
            if (m_baseDir[i] == '\\')
            {
                m_baseDir[i] = '/';
            }
        }

        if (m_baseDir.Size() > 0 && m_baseDir[m_baseDir.Size() - 1] != '/')
        {
            m_baseDir.Append("/", 1);
        }
    }

    void HotReloadManager::SetImportSettingsProvider(ImportSettingsProvider fn, void* userData)
    {
        m_settingsFn = fn;
        m_settingsUserData = userData;
    }

    wax::String HotReloadManager::ToVfsPath(wax::StringView absPath) const
    {
        wax::String result{*m_alloc};
        if (m_baseDir.Size() > 0 && absPath.Size() > m_baseDir.Size() &&
            wax::StringView{absPath.Data(), m_baseDir.Size()} == m_baseDir.View())
        {
            result.Append(absPath.Data() + m_baseDir.Size(), absPath.Size() - m_baseDir.Size());
        }
        else
        {
            result.Append(absPath.Data(), absPath.Size());
        }
        return result;
    }

    size_t HotReloadManager::ProcessChanges(wax::StringView platform)
    {
        HIVE_PROFILE_SCOPE_N("HotReload::ProcessChanges");
        m_lastReloaded.Clear();

        wax::Vector<FileChange> changes{*m_alloc};
        m_watcher->Poll(changes);

        if (changes.Size() == 0)
            return 0;

        // Deduplicate by path, resolve conflicting event types via filesystem check
        wax::Vector<FileChange> deduped{*m_alloc};
        {
            wax::HashMap<wax::String, size_t> seen{*m_alloc};
            for (size_t i = 0; i < changes.Size(); ++i)
            {
                wax::String normalized{*m_alloc};
                normalized.Append(changes[i].m_path.Data(), changes[i].m_path.Size());
                for (size_t c = 0; c < normalized.Size(); ++c)
                {
                    if (normalized[c] == '\\')
                        normalized[c] = '/';
                }

                // Skip .hiveid sidecar events
                if (normalized.Size() > 7)
                {
                    wax::StringView suffix{normalized.Data() + normalized.Size() - 7, 7};
                    if (suffix == wax::StringView{".hiveid", 7})
                        continue;
                }

                // Skip directories (no extension)
                {
                    std::filesystem::path p{normalized.CStr()};
                    if (!p.has_extension())
                        continue;
                }

                auto* existing = seen.Find(normalized);
                if (existing != nullptr)
                {
                    auto& prev = deduped[*existing];
                    bool conflict = (prev.m_kind != changes[i].m_kind);
                    if (conflict)
                    {
                        std::error_code ec;
                        bool onDisk = std::filesystem::exists(std::filesystem::path{normalized.CStr()}, ec);
                        if (!onDisk && m_vfs)
                        {
                            wax::String vfs = ToVfsPath(normalized.View());
                            onDisk = m_vfs->ReadSync(vfs.View()).Size() > 0;
                        }
                        prev.m_kind = onDisk ? FileChangeKind::MODIFIED : FileChangeKind::DELETED;
                    }
                    else
                    {
                        prev.m_kind = changes[i].m_kind;
                    }
                }
                else
                {
                    FileChange entry{};
                    entry.m_path = wax::String{*m_alloc, normalized.View()};
                    entry.m_kind = changes[i].m_kind;
                    seen.Insert(static_cast<wax::String&&>(normalized), deduped.Size());
                    deduped.PushBack(static_cast<FileChange&&>(entry));
                }
            }
        }

        if (deduped.Size() == 0)
            return 0;

        // Classify events
        wax::Vector<size_t> deletedIndices{*m_alloc};
        wax::Vector<size_t> existsIndices{*m_alloc};

        for (size_t i = 0; i < deduped.Size(); ++i)
        {
            const char* kind = deduped[i].m_kind == FileChangeKind::CREATED    ? "CREATED"
                               : deduped[i].m_kind == FileChangeKind::MODIFIED ? "MODIFIED"
                                                                               : "DELETED";
            hive::LogTrace(LOG_HOTRELOAD, "{}: {}", kind, deduped[i].m_path.CStr());

            if (deduped[i].m_kind == FileChangeKind::DELETED)
                deletedIndices.PushBack(i);
            else
                existsIndices.PushBack(i);
        }

        // Rename detection: match deleted by content hash against created/modified
        wax::Vector<bool> deletedConsumed{*m_alloc};
        wax::Vector<bool> existsConsumed{*m_alloc};
        deletedConsumed.Resize(deletedIndices.Size(), false);
        existsConsumed.Resize(existsIndices.Size(), false);

        if (m_vfs && deletedIndices.Size() > 0 && existsIndices.Size() > 0)
        {
            wax::HashMap<ContentHash, size_t> deletedByHash{*m_alloc, deletedIndices.Size()};

            for (size_t di = 0; di < deletedIndices.Size(); ++di)
            {
                wax::String vfsPath = ToVfsPath(deduped[deletedIndices[di]].m_path.View());
                auto* record = m_db->FindByPath(vfsPath.View());
                if (record && record->m_contentHash.IsValid())
                    deletedByHash.Insert(record->m_contentHash, di);
            }

            for (size_t ei = 0; ei < existsIndices.Size(); ++ei)
            {
                wax::String vfsPath = ToVfsPath(deduped[existsIndices[ei]].m_path.View());
                auto fileData = m_vfs->ReadSync(vfsPath.View());
                if (fileData.Size() == 0)
                    continue;

                ContentHash hash = ContentHash::FromData(fileData.Data(), fileData.Size());
                auto* matchIdx = deletedByHash.Find(hash);
                if (!matchIdx)
                    continue;

                size_t di = *matchIdx;
                if (deletedConsumed[di])
                    continue;

                wax::String oldVfs = ToVfsPath(deduped[deletedIndices[di]].m_path.View());
                auto* record = m_db->FindByPath(oldVfs.View());
                if (!record)
                    continue;

                AssetId id = record->m_uuid;
                AssetRecord updated = *record;
                updated.m_path = wax::String{*m_alloc, vfsPath.View()};
                m_db->Update(id, static_cast<AssetRecord&&>(updated));

                auto oldFilename = std::filesystem::path{oldVfs.CStr()}.filename().string();
                auto newFilename = std::filesystem::path{vfsPath.CStr()}.filename().string();
                const char* verb = (oldFilename == newFilename) ? "Moved" : "Renamed";
                hive::LogInfo(LOG_HOTRELOAD, "{}: {} -> {}", verb, oldVfs.CStr(), vfsPath.CStr());

                {
                    wax::String oldAbs{*m_alloc};
                    oldAbs.Append(m_baseDir.Data(), m_baseDir.Size());
                    oldAbs.Append(oldVfs.Data(), oldVfs.Size());
                    wax::String newAbs{*m_alloc};
                    newAbs.Append(m_baseDir.Data(), m_baseDir.Size());
                    newAbs.Append(vfsPath.Data(), vfsPath.Size());

                    auto oldHiveid = std::filesystem::path{oldAbs.CStr()}.concat(".hiveid");
                    std::error_code ec;
                    if (std::filesystem::exists(oldHiveid, ec))
                    {
                        auto newHiveid = std::filesystem::path{newAbs.CStr()}.concat(".hiveid");
                        std::filesystem::rename(oldHiveid, newHiveid, ec);
                        if (!ec)
                            hive::LogInfo(LOG_HOTRELOAD, "{} .hiveid: {} -> {}", verb, oldHiveid.generic_string(),
                                          newHiveid.generic_string());
                    }
                }

                deletedConsumed[di] = true;
                existsConsumed[ei] = true;
                deletedByHash.Remove(hash);
            }
        }

        // Process files that exist (CREATED or MODIFIED)
        wax::Vector<AssetId> toRecook{*m_alloc};

        auto processImport = [&](wax::StringView absPath) {
            wax::String vfsPath = ToVfsPath(absPath);
            wax::StringView lookupPath = vfsPath.View();

            auto* record = m_db->FindByPath(lookupPath);
            if (!record)
            {
                wax::String abs{*m_alloc};
                abs.Append(m_baseDir.Data(), m_baseDir.Size());
                abs.Append(vfsPath.Data(), vfsPath.Size());

                HiveIdData hid{};
                if (!ReadHiveId(abs.CStr(), hid, *m_alloc))
                    return;

                AssetRecord newRec{};
                newRec.m_uuid = hid.m_guid;
                newRec.m_path = wax::String{*m_alloc, lookupPath};
                newRec.m_type = static_cast<wax::String&&>(hid.m_type);
                newRec.m_name = wax::String{*m_alloc};
                auto stem = std::filesystem::path{vfsPath.CStr()}.stem().string();
                newRec.m_name.Append(stem.c_str(), stem.size());
                newRec.m_labels = wax::Vector<wax::String>{*m_alloc};
                m_db->Insert(static_cast<AssetRecord&&>(newRec));

                record = m_db->FindByPath(lookupPath);
                if (!record)
                    return;

                hive::LogInfo(LOG_HOTRELOAD, "Auto-registered: {}", vfsPath.CStr());
            }

            // Skip reimport if content hash unchanged (avoids noise after glTF import)
            // Always import if content hash is invalid (first import, never went through pipeline)
            if (m_vfs && record->m_contentHash.IsValid())
            {
                auto sourceData = m_vfs->ReadSync(lookupPath);
                if (sourceData.Size() > 0)
                {
                    ContentHash currentHash = ContentHash::FromData(sourceData.Data(), sourceData.Size());
                    if (currentHash == record->m_contentHash)
                        return;
                }
            }

            AssetId id = record->m_uuid;

            ImportRequest req;
            req.m_sourcePath = lookupPath;
            req.m_assetId = id;

            ImportOutput result{};
            if (m_settingsFn)
            {
                HiveDocument settings{*m_alloc};
                m_settingsFn(id, lookupPath, settings, m_settingsUserData);
                result = m_importPipe->ImportAsset(req, settings);
            }
            else
            {
                result = m_importPipe->ImportAsset(req);
            }

            if (!result.m_success)
            {
                if (result.m_errorMessage.View().Contains("No importer"))
                    return;
                hive::LogWarning(LOG_HOTRELOAD, "Import failed '{}': {}", vfsPath.CStr(),
                                 result.m_errorMessage.CStr());
                return;
            }

            hive::LogInfo(LOG_HOTRELOAD, "Reimported: {}", vfsPath.CStr());

            m_cookPipe->InvalidateCascade(id);
            toRecook.PushBack(id);

            wax::Vector<AssetId> dependents{*m_alloc};
            m_db->GetDependencyGraph().GetTransitiveDependents(id, DepKind::ALL, dependents);
            for (size_t j = 0; j < dependents.Size(); ++j)
                toRecook.PushBack(dependents[j]);
        };

        for (size_t ei = 0; ei < existsIndices.Size(); ++ei)
        {
            if (existsConsumed[ei])
                continue;
            processImport(deduped[existsIndices[ei]].m_path.View());
        }

        // Process deletions
        for (size_t di = 0; di < deletedIndices.Size(); ++di)
        {
            if (deletedConsumed[di])
                continue;

            wax::String vfsPath = ToVfsPath(deduped[deletedIndices[di]].m_path.View());

            std::filesystem::path fsCheck{vfsPath.CStr()};
            if (!fsCheck.has_extension())
            {
                wax::String prefix{*m_alloc};
                prefix.Append(vfsPath.Data(), vfsPath.Size());
                if (prefix.Size() > 0 && prefix[prefix.Size() - 1] != '/')
                    prefix.Append("/", 1);

                size_t affected{0};
                m_db->ForEach([&](AssetId id, const AssetRecord& rec) {
                    if (rec.m_path.View().StartsWith(prefix.View()))
                    {
                        m_lastReloaded.PushBack(id);
                        ++affected;
                    }
                });

                hive::LogInfo(LOG_HOTRELOAD, "Folder deleted: {} ({} assets affected)", vfsPath.CStr(), affected);
                continue;
            }

            auto* record = m_db->FindByPath(vfsPath.View());
            if (record)
            {
                hive::LogInfo(LOG_HOTRELOAD, "Deleted: {}", vfsPath.CStr());
                m_lastReloaded.PushBack(record->m_uuid);

                wax::String absPath{*m_alloc};
                absPath.Append(m_baseDir.Data(), m_baseDir.Size());
                absPath.Append(vfsPath.Data(), vfsPath.Size());

                auto hiveidPath = std::filesystem::path{absPath.CStr()}.concat(".hiveid");
                std::error_code ec;
                if (std::filesystem::remove(hiveidPath, ec))
                    hive::LogInfo(LOG_HOTRELOAD, "Removed .hiveid: {}", hiveidPath.generic_string());
            }
        }

        if (toRecook.Size() > 0)
        {
            for (size_t i = 0; i < toRecook.Size(); ++i)
            {
                m_lastReloaded.PushBack(toRecook[i]);
            }

            CookRequest cookReq;
            cookReq.m_assets = static_cast<wax::Vector<AssetId>&&>(toRecook);
            cookReq.m_platform = platform;
            cookReq.m_workerCount = 1;
            (void)m_cookPipe->CookAll(cookReq);
        }

        if (m_lastReloaded.Size() > 0)
        {
            hive::LogInfo(LOG_HOTRELOAD, "{} asset(s) reloaded", m_lastReloaded.Size());
        }
        return m_lastReloaded.Size();
    }

    const wax::Vector<AssetId>& HotReloadManager::LastReloaded() const noexcept
    {
        return m_lastReloaded;
    }
} // namespace nectar
