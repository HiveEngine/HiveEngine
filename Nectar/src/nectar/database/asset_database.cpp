#include <nectar/database/asset_database.h>

namespace nectar
{
    AssetDatabase::AssetDatabase(comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , records_{alloc, 256}
        , path_index_{alloc, 256}
        , dep_graph_{alloc}
    {}

    bool AssetDatabase::Insert(AssetRecord record)
    {
        if (records_.Contains(record.uuid)) return false;

        // Check path uniqueness
        if (!record.path.IsEmpty())
        {
            wax::String<> path_key{*alloc_, record.path.View()};
            if (path_index_.Contains(path_key)) return false;
            AssetId uuid = record.uuid;
            path_index_.Insert(static_cast<wax::String<>&&>(path_key), uuid);
        }

        AssetId uuid = record.uuid;
        records_.Insert(uuid, static_cast<AssetRecord&&>(record));
        return true;
    }

    bool AssetDatabase::Remove(AssetId uuid)
    {
        auto* record = records_.Find(uuid);
        if (!record) return false;

        // Remove path index
        if (!record->path.IsEmpty())
        {
            wax::String<> path_key{*alloc_, record->path.View()};
            path_index_.Remove(path_key);
        }

        // Remove from dependency graph
        dep_graph_.RemoveNode(uuid);

        records_.Remove(uuid);
        return true;
    }

    bool AssetDatabase::Update(AssetId uuid, AssetRecord record)
    {
        auto* existing = records_.Find(uuid);
        if (!existing) return false;

        // Update path index if path changed
        if (!existing->path.View().Equals(record.path.View()))
        {
            if (!existing->path.IsEmpty())
            {
                wax::String<> old_key{*alloc_, existing->path.View()};
                path_index_.Remove(old_key);
            }
            if (!record.path.IsEmpty())
            {
                wax::String<> new_key{*alloc_, record.path.View()};
                if (path_index_.Contains(new_key)) return false;
                path_index_.Insert(static_cast<wax::String<>&&>(new_key), uuid);
            }
        }

        *existing = static_cast<AssetRecord&&>(record);
        return true;
    }

    AssetRecord* AssetDatabase::FindByUuid(AssetId uuid)
    {
        return records_.Find(uuid);
    }

    const AssetRecord* AssetDatabase::FindByUuid(AssetId uuid) const
    {
        return records_.Find(uuid);
    }

    AssetRecord* AssetDatabase::FindByPath(wax::StringView path)
    {
        wax::String<> key{*alloc_, path};
        auto* uuid = path_index_.Find(key);
        if (!uuid) return nullptr;
        return records_.Find(*uuid);
    }

    const AssetRecord* AssetDatabase::FindByPath(wax::StringView path) const
    {
        wax::String<> key{*alloc_, path};
        auto* uuid = path_index_.Find(key);
        if (!uuid) return nullptr;
        return records_.Find(*uuid);
    }

    void AssetDatabase::FindByType(wax::StringView type, wax::Vector<AssetRecord*>& out)
    {
        for (auto it = records_.begin(); it != records_.end(); ++it)
        {
            if (it.Value().type.View().Equals(type))
                out.PushBack(&it.Value());
        }
    }

    void AssetDatabase::FindByLabel(wax::StringView label, wax::Vector<AssetRecord*>& out)
    {
        for (auto it = records_.begin(); it != records_.end(); ++it)
        {
            auto& labels = it.Value().labels;
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

    DependencyGraph& AssetDatabase::GetDependencyGraph() noexcept { return dep_graph_; }
    const DependencyGraph& AssetDatabase::GetDependencyGraph() const noexcept { return dep_graph_; }

    size_t AssetDatabase::Count() const noexcept { return records_.Count(); }

    bool AssetDatabase::Contains(AssetId uuid) const { return records_.Contains(uuid); }

    bool AssetDatabase::ContainsPath(wax::StringView path) const
    {
        wax::String<> key{*alloc_, path};
        return path_index_.Contains(key);
    }
}
