#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <nectar/core/asset_id.h>

namespace nectar
{
    class IFileWatcher;
    class AssetDatabase;
    class ImportPipeline;
    class CookPipeline;
    class VirtualFilesystem;
    class HiveDocument;

    /// Callback to provide per-asset import settings during hot reload.
    /// Called with the asset id, VFS path, and an empty HiveDocument to fill.
    using ImportSettingsProvider = void (*)(AssetId id, wax::StringView vfsPath, HiveDocument& outSettings,
                                            void* userData);

    /// Orchestrates hot-reload: file watcher → re-import → re-cook → cascade.
    /// Does NOT touch AssetServer directly — caller handles the asset swap.
    class HotReloadManager
    {
    public:
        HotReloadManager(comb::DefaultAllocator& alloc, IFileWatcher& watcher, AssetDatabase& db,
                         ImportPipeline& importPipe, CookPipeline& cookPipe, VirtualFilesystem* vfs = nullptr);

        /// Watch a source directory for changes.
        void WatchDirectory(wax::StringView dir);

        /// Set the base directory to strip from watcher paths to get VFS paths.
        /// Normalizes backslashes and ensures trailing '/'.
        void SetBaseDirectory(wax::StringView baseDir);

        /// Set a callback for per-asset import settings (e.g. GltfImporter's base_path).
        void SetImportSettingsProvider(ImportSettingsProvider fn, void* userData = nullptr);

        /// Poll for file changes and process reloads.
        /// Returns the number of assets reloaded.
        [[nodiscard]] size_t ProcessChanges(wax::StringView platform);

        /// Get the list of assets reloaded in the last ProcessChanges call.
        [[nodiscard]] const wax::Vector<AssetId>& LastReloaded() const noexcept;

    private:
        comb::DefaultAllocator* m_alloc;
        IFileWatcher* m_watcher;
        AssetDatabase* m_db;
        ImportPipeline* m_importPipe;
        CookPipeline* m_cookPipe;
        VirtualFilesystem* m_vfs;
        wax::Vector<AssetId> m_lastReloaded;
        wax::String m_baseDir;
        ImportSettingsProvider m_settingsFn{nullptr};
        void* m_settingsUserData{nullptr};

        [[nodiscard]] wax::String ToVfsPath(wax::StringView absPath) const;
    };
} // namespace nectar
