#pragma once

#include <nectar/core/asset_id.h>
#include <nectar/database/dep_kind.h>
#include <nectar/database/dependency_graph.h>
#include <wax/containers/vector.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    class AssetDatabase;

    /// Passed to AssetImporter during import.
    /// Allows the importer to declare dependencies it discovers while parsing source data.
    class ImportContext
    {
    public:
        ImportContext(comb::DefaultAllocator& alloc, AssetDatabase& db, AssetId current);

        void DeclareHardDep(AssetId dep);
        void DeclareSoftDep(AssetId dep);
        void DeclareBuildDep(AssetId dep);

        /// Resolve a relative path to an AssetId via the database.
        /// Returns AssetId::Invalid() if the path is not registered.
        [[nodiscard]] AssetId ResolveByPath(wax::StringView relative_path);

        [[nodiscard]] const wax::Vector<DependencyEdge>& GetDeclaredDeps() const noexcept;
        [[nodiscard]] AssetId GetCurrentAsset() const noexcept;

    private:
        void DeclareDep(AssetId dep, DepKind kind);

        AssetDatabase* db_;
        AssetId current_asset_;
        wax::Vector<DependencyEdge> declared_deps_;
    };
}
