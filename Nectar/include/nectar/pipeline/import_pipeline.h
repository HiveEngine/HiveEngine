#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <nectar/database/dep_kind.h>
#include <nectar/database/dependency_graph.h>
#include <nectar/hive/hive_document.h>

namespace nectar
{
    class ImporterRegistry;
    class CasStore;
    class VirtualFilesystem;
    class AssetDatabase;

    struct ImportRequest
    {
        wax::StringView m_sourcePath; // path in VFS
        AssetId m_assetId;            // pre-existing or freshly generated UUID
    };

    struct ImportOutput
    {
        bool m_success{false};
        ContentHash m_contentHash;
        uint32_t m_importVersion{0};
        wax::String m_errorMessage;
        wax::Vector<DependencyEdge> m_dependencies;
    };

    /// Orchestrates asset import: source -> importer -> CAS + database.
    class HIVE_API ImportPipeline
    {
    public:
        ImportPipeline(comb::DefaultAllocator& alloc, ImporterRegistry& registry, CasStore& cas, VirtualFilesystem& vfs,
                       AssetDatabase& db);

        /// Import an asset from VFS with default (empty) HiveDocument settings.
        [[nodiscard]] ImportOutput ImportAsset(const ImportRequest& request);

        /// Import an asset with explicit HiveDocument settings.
        [[nodiscard]] ImportOutput ImportAsset(const ImportRequest& request, const HiveDocument& settings);

        /// Check if an asset needs re-import (version or content changed).
        [[nodiscard]] bool NeedsReimport(AssetId id) const;

        /// Scan all DB assets, return those needing re-import.
        void ScanOutdated(wax::Vector<AssetId>& out) const;

        /// Re-import a batch. Returns count of successful re-imports.
        [[nodiscard]] size_t ReimportOutdated(const wax::Vector<AssetId>& assets);

    private:
        comb::DefaultAllocator* m_alloc;
        ImporterRegistry* m_registry;
        CasStore* m_cas;
        VirtualFilesystem* m_vfs;
        AssetDatabase* m_db;
    };
} // namespace nectar
