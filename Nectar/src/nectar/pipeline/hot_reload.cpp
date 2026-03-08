#include <nectar/database/asset_database.h>
#include <nectar/database/dependency_graph.h>
#include <nectar/hive/hive_document.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/hot_reload.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/watcher/file_watcher.h>

namespace nectar
{
    HotReloadManager::HotReloadManager(comb::DefaultAllocator& alloc, IFileWatcher& watcher, AssetDatabase& db,
                                       ImportPipeline& importPipe, CookPipeline& cookPipe)
        : m_alloc{&alloc}
        , m_watcher{&watcher}
        , m_db{&db}
        , m_importPipe{&importPipe}
        , m_cookPipe{&cookPipe}
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

        wax::Vector<AssetId> toRecook{*m_alloc};

        for (size_t i = 0; i < changes.Size(); ++i)
        {
            if (changes[i].m_kind == FileChangeKind::DELETED)
            {
                continue;
            }

            wax::StringView lookupPath = changes[i].m_path.View();
            wax::String vfsBuf{*m_alloc};
            if (m_baseDir.Size() > 0)
            {
                auto abs = changes[i].m_path.View();
                if (abs.Size() > m_baseDir.Size() && wax::StringView{abs.Data(), m_baseDir.Size()} == m_baseDir.View())
                {
                    vfsBuf.Append(abs.Data() + m_baseDir.Size(), abs.Size() - m_baseDir.Size());
                    lookupPath = vfsBuf.View();
                }
            }

            auto* record = m_db->FindByPath(lookupPath);
            if (!record)
            {
                continue;
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
                continue;
            }

            m_cookPipe->InvalidateCascade(id);
            toRecook.PushBack(id);

            wax::Vector<AssetId> dependents{*m_alloc};
            m_db->GetDependencyGraph().GetTransitiveDependents(id, DepKind::ALL, dependents);
            for (size_t j = 0; j < dependents.Size(); ++j)
            {
                toRecook.PushBack(dependents[j]);
            }
        }

        if (toRecook.Size() == 0)
        {
            return 0;
        }

        for (size_t i = 0; i < toRecook.Size(); ++i)
        {
            m_lastReloaded.PushBack(toRecook[i]);
        }

        CookRequest cookReq;
        cookReq.m_assets = static_cast<wax::Vector<AssetId>&&>(toRecook);
        cookReq.m_platform = platform;
        cookReq.m_workerCount = 1;
        (void)m_cookPipe->CookAll(cookReq);

        return m_lastReloaded.Size();
    }

    const wax::Vector<AssetId>& HotReloadManager::LastReloaded() const noexcept
    {
        return m_lastReloaded;
    }
} // namespace nectar
