#pragma once

#include <nectar/core/asset_id.h>
#include <nectar/database/dep_kind.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>
#include <comb/default_allocator.h>

namespace nectar
{
    struct DependencyEdge
    {
        AssetId from;
        AssetId to;
        DepKind kind;
    };

    /// Directed Acyclic Graph of asset dependencies.
    /// Double-indexed: forward (from -> deps) and reverse (to -> dependants).
    class DependencyGraph
    {
    public:
        explicit DependencyGraph(comb::DefaultAllocator& alloc);

        // -- Construction --

        /// Add an edge. Returns false if it would create a cycle.
        bool AddEdge(AssetId from, AssetId to, DepKind kind);

        /// Remove a specific edge. Returns false if not found.
        bool RemoveEdge(AssetId from, AssetId to);

        /// Remove a node and all its edges (both directions).
        void RemoveNode(AssetId id);

        // -- Direct queries --

        /// "What does `id` depend on?" (outgoing edges matching filter)
        void GetDependencies(AssetId id, DepKind filter,
                             wax::Vector<AssetId>& out) const;

        /// "Who depends on `id`?" (incoming edges matching filter)
        void GetDependents(AssetId id, DepKind filter,
                           wax::Vector<AssetId>& out) const;

        // -- Transitive queries (DFS) --

        void GetTransitiveDependencies(AssetId id, DepKind filter,
                                       wax::Vector<AssetId>& out) const;

        void GetTransitiveDependents(AssetId id, DepKind filter,
                                     wax::Vector<AssetId>& out) const;

        // -- Validation --

        [[nodiscard]] bool HasCycle() const;

        // -- Cook ordering --

        /// Topological sort (Kahn's algorithm). Returns false if cycle detected.
        [[nodiscard]] bool TopologicalSort(wax::Vector<AssetId>& order) const;

        /// Topological sort grouped by parallelism level.
        /// Level 0 = no dependencies, level 1 = depends only on level 0, etc.
        /// Returns false if cycle detected.
        [[nodiscard]] bool TopologicalSortLevels(wax::Vector<wax::Vector<AssetId>>& levels) const;

        // -- Stats --

        [[nodiscard]] size_t NodeCount() const;
        [[nodiscard]] size_t EdgeCount() const;
        [[nodiscard]] bool HasNode(AssetId id) const;
        [[nodiscard]] bool HasEdge(AssetId from, AssetId to) const;

    private:
        /// DFS reachability check: can we reach `target` starting from `start`?
        bool CanReach(AssetId start, AssetId target) const;

        comb::DefaultAllocator* alloc_;
        wax::HashMap<AssetId, wax::Vector<DependencyEdge>> forward_;
        wax::HashMap<AssetId, wax::Vector<DependencyEdge>> reverse_;
    };
}
