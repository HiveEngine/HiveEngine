#pragma once

#include <nectar/core/asset_id.h>
#include <wax/containers/vector.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    class IFileWatcher;
    class AssetDatabase;
    class ImportPipeline;
    class CookPipeline;
    class CasStore;

    /// Orchestrates hot-reload: file watcher → re-import → re-cook → cascade.
    /// Does NOT touch AssetServer directly — caller handles the asset swap.
    class HotReloadManager
    {
    public:
        HotReloadManager(comb::DefaultAllocator& alloc,
                         IFileWatcher& watcher,
                         AssetDatabase& db,
                         ImportPipeline& import_pipe,
                         CookPipeline& cook_pipe);

        /// Watch a source directory for changes.
        void WatchDirectory(wax::StringView dir);

        /// Poll for file changes and process reloads.
        /// Returns the number of assets reloaded.
        size_t ProcessChanges(wax::StringView platform);

        /// Get the list of assets reloaded in the last ProcessChanges call.
        [[nodiscard]] const wax::Vector<AssetId>& LastReloaded() const noexcept;

    private:
        comb::DefaultAllocator* alloc_;
        IFileWatcher* watcher_;
        AssetDatabase* db_;
        ImportPipeline* import_pipe_;
        CookPipeline* cook_pipe_;
        wax::Vector<AssetId> last_reloaded_;
    };
}
