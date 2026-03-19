#include <hive/core/log.h>

#include <wax/containers/hash_map.h>

#include <nectar/core/content_hash.h>
#include <nectar/database/asset_database.h>
#include <nectar/database/dependency_graph.h>
#include <nectar/hive/hive_document.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/hot_reload.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/watcher/file_watcher.h>

#include <filesystem>
#include <string>

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
        {
            return 0;
        }

        for (size_t i = 0; i < changes.Size(); ++i)
        {
            const char* kind = changes[i].m_kind == FileChangeKind::CREATED    ? "CREATED"
                               : changes[i].m_kind == FileChangeKind::MODIFIED ? "MODIFIED"
                                                                               : "DELETED";
            hive::LogTrace(LOG_HOTRELOAD, "{}: {}", kind, changes[i].m_path.CStr());
        }

        // Separate events by kind for rename detection
        wax::Vector<size_t> deletedIndices{*m_alloc};
        wax::Vector<size_t> createdIndices{*m_alloc};
        wax::Vector<size_t> modifiedIndices{*m_alloc};

        for (size_t i = 0; i < changes.Size(); ++i)
        {
            switch (changes[i].m_kind)
            {
                case FileChangeKind::DELETED:
                    deletedIndices.PushBack(i);
                    break;
                case FileChangeKind::CREATED:
                    createdIndices.PushBack(i);
                    break;
                case FileChangeKind::MODIFIED:
                    modifiedIndices.PushBack(i);
                    break;
            }
        }

        // Detect renames: match DELETED records by content hash against CREATED files
        wax::Vector<bool> deletedConsumed{*m_alloc};
        wax::Vector<bool> createdConsumed{*m_alloc};
        deletedConsumed.Resize(deletedIndices.Size(), false);
        createdConsumed.Resize(createdIndices.Size(), false);

        if (m_vfs && deletedIndices.Size() > 0 && createdIndices.Size() > 0)
        {
            // Build a map from content hash -> index into deletedIndices for DB records
            wax::HashMap<ContentHash, size_t> deletedByHash{*m_alloc, deletedIndices.Size()};

            for (size_t di = 0; di < deletedIndices.Size(); ++di)
            {
                wax::String vfsPath = ToVfsPath(changes[deletedIndices[di]].m_path.View());
                auto* record = m_db->FindByPath(vfsPath.View());
                if (record && record->m_contentHash.IsValid())
                {
                    deletedByHash.Insert(record->m_contentHash, di);
                }
            }

            for (size_t ci = 0; ci < createdIndices.Size(); ++ci)
            {
                wax::String vfsPath = ToVfsPath(changes[createdIndices[ci]].m_path.View());
                auto fileData = m_vfs->ReadSync(vfsPath.View());
                if (fileData.Size() == 0)
                    continue;

                ContentHash createdHash = ContentHash::FromData(fileData.Data(), fileData.Size());
                auto* matchIdx = deletedByHash.Find(createdHash);
                if (!matchIdx)
                    continue;

                size_t di = *matchIdx;
                if (deletedConsumed[di])
                    continue;

                wax::String oldVfs = ToVfsPath(changes[deletedIndices[di]].m_path.View());
                auto* record = m_db->FindByPath(oldVfs.View());
                if (!record)
                    continue;

                AssetId id = record->m_uuid;
                wax::String newVfs = ToVfsPath(changes[createdIndices[ci]].m_path.View());
                AssetRecord updated = *record;
                updated.m_path = static_cast<wax::String&&>(vfsPath);
                m_db->Update(id, static_cast<AssetRecord&&>(updated));

                auto oldFilename = std::filesystem::path{oldVfs.CStr()}.filename().string();
                auto newFilename = std::filesystem::path{newVfs.CStr()}.filename().string();
                const char* verb = (oldFilename == newFilename) ? "Moved" : "Renamed";
                hive::LogInfo(LOG_HOTRELOAD, "{}: {} -> {}", verb, oldVfs.CStr(), newVfs.CStr());

                deletedConsumed[di] = true;
                createdConsumed[ci] = true;
                deletedByHash.Remove(createdHash);
            }
        }

        // Process unmatched CREATED and MODIFIED events as imports
        wax::Vector<AssetId> toRecook{*m_alloc};

        auto processImport = [&](wax::StringView rawPath) {
            wax::String vfsPath = ToVfsPath(rawPath);
            wax::StringView lookupPath = vfsPath.View();

            auto* record = m_db->FindByPath(lookupPath);
            AssetId id;
            if (record)
            {
                id = record->m_uuid;
            }
            else
            {
                auto pathHash = ContentHash::FromData(lookupPath.Data(), lookupPath.Size());
                id = AssetId{pathHash.High(), pathHash.Low()};
            }

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
                hive::LogWarning(LOG_HOTRELOAD, "Import failed '{}': {}", vfsPath.CStr(), result.m_errorMessage.CStr());
                return;
            }

            hive::LogInfo(LOG_HOTRELOAD, "Reimported: {}", vfsPath.CStr());

            m_cookPipe->InvalidateCascade(id);
            toRecook.PushBack(id);

            wax::Vector<AssetId> dependents{*m_alloc};
            m_db->GetDependencyGraph().GetTransitiveDependents(id, DepKind::ALL, dependents);
            for (size_t j = 0; j < dependents.Size(); ++j)
            {
                toRecook.PushBack(dependents[j]);
            }
        };

        for (size_t di = 0; di < deletedIndices.Size(); ++di)
        {
            if (deletedConsumed[di])
                continue;
            wax::String vfsPath = ToVfsPath(changes[deletedIndices[di]].m_path.View());
            auto* record = m_db->FindByPath(vfsPath.View());
            if (record)
            {
                hive::LogInfo(LOG_HOTRELOAD, "Deleted: {}", vfsPath.CStr());
                m_lastReloaded.PushBack(record->m_uuid);
            }
        }

        for (size_t ci = 0; ci < createdIndices.Size(); ++ci)
        {
            if (createdConsumed[ci])
                continue;
            processImport(changes[createdIndices[ci]].m_path.View());
        }

        for (size_t mi = 0; mi < modifiedIndices.Size(); ++mi)
        {
            processImport(changes[modifiedIndices[mi]].m_path.View());
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
