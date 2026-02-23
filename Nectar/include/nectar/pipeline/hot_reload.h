#pragma once

#include <nectar/core/asset_id.h>
#include <wax/containers/vector.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    class IFileWatcher;
    class AssetDatabase;
    class ImportPipeline;
    class CookPipeline;
    class HiveDocument;

    /// Callback to provide per-asset import settings during hot reload.
    /// Called with the asset id, VFS path, and an empty HiveDocument to fill.
    using ImportSettingsProvider = void (*)(AssetId id, wax::StringView vfs_path,
                                           HiveDocument& out_settings, void* user_data);

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

        /// Set the base directory to strip from watcher paths to get VFS paths.
        /// Normalizes backslashes and ensures trailing '/'.
        void SetBaseDirectory(wax::StringView base_dir);

        /// Set a callback for per-asset import settings (e.g. GltfImporter's base_path).
        void SetImportSettingsProvider(ImportSettingsProvider fn, void* user_data = nullptr);

        /// Poll for file changes and process reloads.
        /// Returns the number of assets reloaded.
        [[nodiscard]] size_t ProcessChanges(wax::StringView platform);

        /// Get the list of assets reloaded in the last ProcessChanges call.
        [[nodiscard]] const wax::Vector<AssetId>& LastReloaded() const noexcept;

    private:
        comb::DefaultAllocator* alloc_;
        IFileWatcher* watcher_;
        AssetDatabase* db_;
        ImportPipeline* import_pipe_;
        CookPipeline* cook_pipe_;
        wax::Vector<AssetId> last_reloaded_;
        wax::String<> base_dir_;
        ImportSettingsProvider settings_fn_{nullptr};
        void* settings_user_data_{nullptr};
    };
}
