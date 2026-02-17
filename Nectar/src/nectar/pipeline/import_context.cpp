#include <nectar/pipeline/import_context.h>
#include <nectar/database/asset_database.h>

namespace nectar
{
    ImportContext::ImportContext(comb::DefaultAllocator& alloc, AssetDatabase& db, AssetId current)
        : db_{&db}
        , current_asset_{current}
        , declared_deps_{alloc}
    {}

    void ImportContext::DeclareHardDep(AssetId dep)
    {
        DeclareDep(dep, DepKind::Hard);
    }

    void ImportContext::DeclareSoftDep(AssetId dep)
    {
        DeclareDep(dep, DepKind::Soft);
    }

    void ImportContext::DeclareBuildDep(AssetId dep)
    {
        DeclareDep(dep, DepKind::Build);
    }

    AssetId ImportContext::ResolveByPath(wax::StringView relative_path)
    {
        auto* record = db_->FindByPath(relative_path);
        if (!record) return AssetId::Invalid();
        return record->uuid;
    }

    const wax::Vector<DependencyEdge>& ImportContext::GetDeclaredDeps() const noexcept
    {
        return declared_deps_;
    }

    AssetId ImportContext::GetCurrentAsset() const noexcept
    {
        return current_asset_;
    }

    void ImportContext::DeclareDep(AssetId dep, DepKind kind)
    {
        if (!dep.IsValid()) return;
        declared_deps_.PushBack(DependencyEdge{current_asset_, dep, kind});
    }
}
