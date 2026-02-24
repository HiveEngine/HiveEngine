#pragma once

#include <nectar/database/asset_record.h>
#include <nectar/database/dependency_graph.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// In-memory registry of all known assets.
    /// Dual-indexed: by UUID (primary) and by path (secondary).
    class AssetDatabase
    {
    public:
        explicit AssetDatabase(comb::DefaultAllocator& alloc);

        // -- CRUD --

        /// Insert a record. Returns false if uuid already exists or path is taken.
        bool Insert(AssetRecord record);

        /// Remove a record by uuid. Returns false if not found.
        bool Remove(AssetId uuid);

        /// Update an existing record. Returns false if uuid not found.
        bool Update(AssetId uuid, AssetRecord record);

        // -- Lookup O(1) --

        [[nodiscard]] AssetRecord* FindByUuid(AssetId uuid);
        [[nodiscard]] const AssetRecord* FindByUuid(AssetId uuid) const;
        [[nodiscard]] AssetRecord* FindByPath(wax::StringView path);
        [[nodiscard]] const AssetRecord* FindByPath(wax::StringView path) const;

        // -- Queries --

        void FindByType(wax::StringView type, wax::Vector<AssetRecord*>& out);
        void FindByLabel(wax::StringView label, wax::Vector<AssetRecord*>& out);

        // -- Dependency graph --

        [[nodiscard]] DependencyGraph& GetDependencyGraph() noexcept;
        [[nodiscard]] const DependencyGraph& GetDependencyGraph() const noexcept;

        // -- Iteration --

        template<typename F>
        void ForEach(F&& fn) const
        {
            for (auto it = records_.begin(); it != records_.end(); ++it)
                fn(it.Key(), it.Value());
        }

        // -- Stats --

        [[nodiscard]] size_t Count() const noexcept;
        [[nodiscard]] bool Contains(AssetId uuid) const;
        [[nodiscard]] bool ContainsPath(wax::StringView path) const;

    private:
        comb::DefaultAllocator* alloc_;
        wax::HashMap<AssetId, AssetRecord> records_;
        wax::HashMap<wax::String<>, AssetId> path_index_;
        DependencyGraph dep_graph_;
    };
}
