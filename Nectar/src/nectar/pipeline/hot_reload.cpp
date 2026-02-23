#include <nectar/pipeline/hot_reload.h>
#include <nectar/watcher/file_watcher.h>
#include <nectar/database/asset_database.h>
#include <nectar/database/dependency_graph.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/hive/hive_document.h>
#include <hive/profiling/profiler.h>

namespace nectar
{
    HotReloadManager::HotReloadManager(comb::DefaultAllocator& alloc,
                                         IFileWatcher& watcher,
                                         AssetDatabase& db,
                                         ImportPipeline& import_pipe,
                                         CookPipeline& cook_pipe)
        : alloc_{&alloc}
        , watcher_{&watcher}
        , db_{&db}
        , import_pipe_{&import_pipe}
        , cook_pipe_{&cook_pipe}
        , last_reloaded_{alloc}
        , base_dir_{alloc}
    {}

    void HotReloadManager::WatchDirectory(wax::StringView dir)
    {
        watcher_->Watch(dir);
    }

    void HotReloadManager::SetBaseDirectory(wax::StringView base_dir)
    {
        base_dir_ = wax::String<>{*alloc_};
        base_dir_.Append(base_dir.Data(), base_dir.Size());

        // Normalize backslashes
        for (size_t i = 0; i < base_dir_.Size(); ++i)
            if (base_dir_[i] == '\\') base_dir_[i] = '/';

        // Trailing slash
        if (base_dir_.Size() > 0 && base_dir_[base_dir_.Size() - 1] != '/')
            base_dir_.Append("/", 1);
    }

    void HotReloadManager::SetImportSettingsProvider(ImportSettingsProvider fn, void* user_data)
    {
        settings_fn_ = fn;
        settings_user_data_ = user_data;
    }

    size_t HotReloadManager::ProcessChanges(wax::StringView platform)
    {
        HIVE_PROFILE_SCOPE_N("HotReload::ProcessChanges");
        last_reloaded_.Clear();

        wax::Vector<FileChange> changes{*alloc_};
        watcher_->Poll(changes);

        if (changes.Size() == 0) return 0;

        // Collect affected asset IDs
        wax::Vector<AssetId> to_recook{*alloc_};

        for (size_t i = 0; i < changes.Size(); ++i)
        {
            if (changes[i].kind == FileChangeKind::Deleted) continue;

            // Strip base directory to get VFS path
            wax::StringView lookup_path = changes[i].path.View();
            wax::String<> vfs_buf{*alloc_};
            if (base_dir_.Size() > 0)
            {
                auto abs = changes[i].path.View();
                if (abs.Size() > base_dir_.Size() &&
                    wax::StringView{abs.Data(), base_dir_.Size()} == base_dir_.View())
                {
                    vfs_buf.Append(abs.Data() + base_dir_.Size(),
                                   abs.Size() - base_dir_.Size());
                    lookup_path = vfs_buf.View();
                }
            }

            auto* record = db_->FindByPath(lookup_path);
            if (!record) continue;

            AssetId id = record->uuid;

            // Re-import (with optional settings)
            ImportRequest req;
            req.source_path = lookup_path;
            req.asset_id = id;

            ImportOutput result{};
            if (settings_fn_)
            {
                HiveDocument settings{*alloc_};
                settings_fn_(id, lookup_path, settings, settings_user_data_);
                result = import_pipe_->ImportAsset(req, settings);
            }
            else
            {
                result = import_pipe_->ImportAsset(req);
            }
            if (!result.success) continue;

            // Invalidate cook cache for this asset and all dependents
            cook_pipe_->InvalidateCascade(id);

            // Collect this asset + transitive dependents for re-cook
            to_recook.PushBack(id);

            wax::Vector<AssetId> dependents{*alloc_};
            db_->GetDependencyGraph().GetTransitiveDependents(id, DepKind::All, dependents);
            for (size_t j = 0; j < dependents.Size(); ++j)
                to_recook.PushBack(dependents[j]);
        }

        if (to_recook.Size() == 0) return 0;

        // Save the list before cooking (CookAll may consume the vector)
        for (size_t i = 0; i < to_recook.Size(); ++i)
            last_reloaded_.PushBack(to_recook[i]);

        // Re-cook
        CookRequest cook_req;
        cook_req.assets = static_cast<wax::Vector<AssetId>&&>(to_recook);
        cook_req.platform = platform;
        cook_req.worker_count = 1;
        (void)cook_pipe_->CookAll(cook_req);

        return last_reloaded_.Size();
    }

    const wax::Vector<AssetId>& HotReloadManager::LastReloaded() const noexcept
    {
        return last_reloaded_;
    }
}
