#include <nectar/pipeline/import_context.h>

#include <nectar/database/asset_database.h>

namespace nectar
{
    ImportContext::ImportContext(comb::DefaultAllocator& alloc, AssetDatabase& db, AssetId current)
        : m_alloc{&alloc}
        , m_db{&db}
        , m_currentAsset{current}
        , m_declaredDeps{alloc}
    {
    }

    void ImportContext::DeclareHardDep(AssetId dep)
    {
        DeclareDep(dep, DepKind::HARD);
    }

    void ImportContext::DeclareSoftDep(AssetId dep)
    {
        DeclareDep(dep, DepKind::SOFT);
    }

    void ImportContext::DeclareBuildDep(AssetId dep)
    {
        DeclareDep(dep, DepKind::BUILD);
    }

    AssetId ImportContext::ResolveByPath(wax::StringView relativePath)
    {
        auto* record = m_db->FindByPath(relativePath);
        if (!record)
            return AssetId::Invalid();
        return record->m_uuid;
    }

    const wax::Vector<DependencyEdge>& ImportContext::GetDeclaredDeps() const noexcept
    {
        return m_declaredDeps;
    }

    AssetId ImportContext::GetCurrentAsset() const noexcept
    {
        return m_currentAsset;
    }

    void ImportContext::DeclareDep(AssetId dep, DepKind kind)
    {
        if (!dep.IsValid())
            return;
        m_declaredDeps.PushBack(DependencyEdge{m_currentAsset, dep, kind});
    }
} // namespace nectar
