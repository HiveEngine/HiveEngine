#include <nectar/database/asset_database.h>

namespace nectar
{
    AssetDatabase::AssetDatabase(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_records{alloc, 256}
        , m_pathIndex{alloc, 256}
        , m_depGraph{alloc} {}

    bool AssetDatabase::Insert(AssetRecord record) {
        if (m_records.Contains(record.m_uuid))
            return false;

        if (!record.m_path.IsEmpty())
        {
            wax::String pathKey{*m_alloc, record.m_path.View()};
            if (m_pathIndex.Contains(pathKey))
                return false;

            AssetId uuid = record.m_uuid;
            m_pathIndex.Insert(static_cast<wax::String&&>(pathKey), uuid);
        }

        AssetId uuid = record.m_uuid;
        m_records.Insert(uuid, static_cast<AssetRecord&&>(record));
        return true;
    }

    bool AssetDatabase::Remove(AssetId uuid) {
        auto* record = m_records.Find(uuid);
        if (!record)
            return false;

        // Remove path index
        if (!record->m_path.IsEmpty())
        {
            wax::String pathKey{*m_alloc, record->m_path.View()};
            m_pathIndex.Remove(pathKey);
        }

        // Remove from dependency graph
        m_depGraph.RemoveNode(uuid);

        m_records.Remove(uuid);
        return true;
    }

    bool AssetDatabase::Update(AssetId uuid, AssetRecord record) {
        auto* existing = m_records.Find(uuid);
        if (!existing)
            return false;

        if (!existing->m_path.View().Equals(record.m_path.View()))
        {
            if (!existing->m_path.IsEmpty())
            {
                wax::String oldKey{*m_alloc, existing->m_path.View()};
                m_pathIndex.Remove(oldKey);
            }
            if (!record.m_path.IsEmpty())
            {
                wax::String newKey{*m_alloc, record.m_path.View()};
                if (m_pathIndex.Contains(newKey))
                    return false;

                m_pathIndex.Insert(static_cast<wax::String&&>(newKey), uuid);
            }
        }

        *existing = static_cast<AssetRecord&&>(record);
        return true;
    }

    AssetRecord* AssetDatabase::FindByUuid(AssetId uuid) {
        return m_records.Find(uuid);
    }

    const AssetRecord* AssetDatabase::FindByUuid(AssetId uuid) const {
        return m_records.Find(uuid);
    }

    AssetRecord* AssetDatabase::FindByPath(wax::StringView path) {
        wax::String key{*m_alloc, path};
        auto* uuid = m_pathIndex.Find(key);
        if (!uuid)
            return nullptr;

        return m_records.Find(*uuid);
    }

    const AssetRecord* AssetDatabase::FindByPath(wax::StringView path) const {
        wax::String key{*m_alloc, path};
        auto* uuid = m_pathIndex.Find(key);
        if (!uuid)
            return nullptr;

        return m_records.Find(*uuid);
    }

    void AssetDatabase::FindByType(wax::StringView type, wax::Vector<AssetRecord*>& out) {
        for (auto it = m_records.Begin(); it != m_records.End(); ++it)
        {
            if (it.Value().m_type.View().Equals(type))
                out.PushBack(&it.Value());
        }
    }

    void AssetDatabase::FindByLabel(wax::StringView label, wax::Vector<AssetRecord*>& out) {
        for (auto it = m_records.Begin(); it != m_records.End(); ++it)
        {
            auto& labels = it.Value().m_labels;
            for (size_t i = 0; i < labels.Size(); ++i)
            {
                if (labels[i].View().Equals(label))
                {
                    out.PushBack(&it.Value());
                    break;
                }
            }
        }
    }

    DependencyGraph& AssetDatabase::GetDependencyGraph() noexcept {
        return m_depGraph;
    }
    const DependencyGraph& AssetDatabase::GetDependencyGraph() const noexcept {
        return m_depGraph;
    }

    size_t AssetDatabase::Count() const noexcept {
        return m_records.Count();
    }

    bool AssetDatabase::Contains(AssetId uuid) const {
        return m_records.Contains(uuid);
    }

    bool AssetDatabase::ContainsPath(wax::StringView path) const {
        wax::String key{*m_alloc, path};
        return m_pathIndex.Contains(key);
    }
} // namespace nectar
