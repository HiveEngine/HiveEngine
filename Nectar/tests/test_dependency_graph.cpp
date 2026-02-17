#include <larvae/larvae.h>
#include <nectar/database/dependency_graph.h>
#include <nectar/core/asset_id.h>
#include <comb/default_allocator.h>
#include <cstring>

namespace {

    auto& GetGraphAlloc()
    {
        static comb::ModuleAllocator alloc{"TestDepGraph", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // Deterministic test IDs (avoid random UUIDs in tests)
    nectar::AssetId MakeId(uint64_t v)
    {
        uint8_t bytes[16] = {};
        std::memcpy(bytes, &v, sizeof(v));
        return nectar::AssetId::FromBytes(bytes);
    }

    // =========================================================================
    // Construction
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarDepGraph", "EmptyGraph", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        larvae::AssertEqual(graph.NodeCount(), size_t{0});
        larvae::AssertEqual(graph.EdgeCount(), size_t{0});
    });

    auto t2 = larvae::RegisterTest("NectarDepGraph", "AddEdge", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        larvae::AssertTrue(graph.AddEdge(a, b, nectar::DepKind::Hard));
        larvae::AssertEqual(graph.NodeCount(), size_t{2});
        larvae::AssertEqual(graph.EdgeCount(), size_t{1});
        larvae::AssertTrue(graph.HasEdge(a, b));
    });

    auto t3 = larvae::RegisterTest("NectarDepGraph", "AddDuplicateEdgeRejected", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        larvae::AssertTrue(graph.AddEdge(a, b, nectar::DepKind::Hard));
        larvae::AssertFalse(graph.AddEdge(a, b, nectar::DepKind::Soft)); // same edge, different kind
        larvae::AssertEqual(graph.EdgeCount(), size_t{1});
    });

    auto t4 = larvae::RegisterTest("NectarDepGraph", "SelfLoopRejected", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        larvae::AssertFalse(graph.AddEdge(a, a, nectar::DepKind::Hard));
        larvae::AssertEqual(graph.EdgeCount(), size_t{0});
    });

    auto t5 = larvae::RegisterTest("NectarDepGraph", "RemoveEdge", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        larvae::AssertTrue(graph.RemoveEdge(a, b));
        larvae::AssertFalse(graph.HasEdge(a, b));
        larvae::AssertEqual(graph.EdgeCount(), size_t{0});
    });

    auto t6 = larvae::RegisterTest("NectarDepGraph", "RemoveNonExistentEdge", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        larvae::AssertFalse(graph.RemoveEdge(a, b));
    });

    auto t7 = larvae::RegisterTest("NectarDepGraph", "RemoveNode", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Hard);

        graph.RemoveNode(b);
        larvae::AssertFalse(graph.HasNode(b));
        larvae::AssertFalse(graph.HasEdge(a, b));
        larvae::AssertFalse(graph.HasEdge(b, c));
    });

    // =========================================================================
    // Direct queries
    // =========================================================================

    auto t8 = larvae::RegisterTest("NectarDepGraph", "GetDependencies", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(a, c, nectar::DepKind::Soft);

        wax::Vector<nectar::AssetId> deps{alloc};
        graph.GetDependencies(a, nectar::DepKind::All, deps);
        larvae::AssertEqual(deps.Size(), size_t{2});
    });

    auto t9 = larvae::RegisterTest("NectarDepGraph", "GetDependenciesFiltered", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(a, c, nectar::DepKind::Soft);

        wax::Vector<nectar::AssetId> hard_deps{alloc};
        graph.GetDependencies(a, nectar::DepKind::Hard, hard_deps);
        larvae::AssertEqual(hard_deps.Size(), size_t{1});
        larvae::AssertTrue(hard_deps[0] == b);

        wax::Vector<nectar::AssetId> soft_deps{alloc};
        graph.GetDependencies(a, nectar::DepKind::Soft, soft_deps);
        larvae::AssertEqual(soft_deps.Size(), size_t{1});
        larvae::AssertTrue(soft_deps[0] == c);
    });

    auto t10 = larvae::RegisterTest("NectarDepGraph", "GetDependents", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, c, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Hard);

        wax::Vector<nectar::AssetId> dependents{alloc};
        graph.GetDependents(c, nectar::DepKind::All, dependents);
        larvae::AssertEqual(dependents.Size(), size_t{2});
    });

    // =========================================================================
    // Transitive queries
    // =========================================================================

    auto t11 = larvae::RegisterTest("NectarDepGraph", "TransitiveDependenciesChain", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        auto d = MakeId(4);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Hard);
        graph.AddEdge(c, d, nectar::DepKind::Hard);

        wax::Vector<nectar::AssetId> deps{alloc};
        graph.GetTransitiveDependencies(a, nectar::DepKind::All, deps);
        larvae::AssertEqual(deps.Size(), size_t{3}); // b, c, d
    });

    auto t12 = larvae::RegisterTest("NectarDepGraph", "TransitiveDependentsDiamond", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        //     a
        //    / \
        //   b   c
        //    \ /
        //     d
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        auto d = MakeId(4);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(a, c, nectar::DepKind::Hard);
        graph.AddEdge(b, d, nectar::DepKind::Hard);
        graph.AddEdge(c, d, nectar::DepKind::Hard);

        wax::Vector<nectar::AssetId> deps{alloc};
        graph.GetTransitiveDependencies(a, nectar::DepKind::All, deps);
        larvae::AssertEqual(deps.Size(), size_t{3}); // b, c, d (no duplicates)
    });

    auto t13 = larvae::RegisterTest("NectarDepGraph", "TransitiveDependentsReverse", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(b, a, nectar::DepKind::Hard);
        graph.AddEdge(c, b, nectar::DepKind::Hard);

        wax::Vector<nectar::AssetId> dependents{alloc};
        graph.GetTransitiveDependents(a, nectar::DepKind::All, dependents);
        larvae::AssertEqual(dependents.Size(), size_t{2}); // b, c
    });

    auto t14 = larvae::RegisterTest("NectarDepGraph", "TransitiveFilteredByKind", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Soft); // soft, won't follow with Hard filter

        wax::Vector<nectar::AssetId> deps{alloc};
        graph.GetTransitiveDependencies(a, nectar::DepKind::Hard, deps);
        larvae::AssertEqual(deps.Size(), size_t{1}); // only b
    });

    // =========================================================================
    // Cycle detection
    // =========================================================================

    auto t15 = larvae::RegisterTest("NectarDepGraph", "SimpleCycleRejected", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        larvae::AssertFalse(graph.AddEdge(b, a, nectar::DepKind::Hard)); // would create cycle
    });

    auto t16 = larvae::RegisterTest("NectarDepGraph", "TransitiveCycleRejected", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Hard);
        larvae::AssertFalse(graph.AddEdge(c, a, nectar::DepKind::Hard)); // would create a->b->c->a
    });

    auto t17 = larvae::RegisterTest("NectarDepGraph", "NoCycleNonFalsePositive", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, c, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Hard);
        // a -> c and b -> c is fine (diamond, not cycle)
        larvae::AssertFalse(graph.HasCycle());
    });

    auto t18 = larvae::RegisterTest("NectarDepGraph", "HasCycleOnCleanGraph", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        larvae::AssertFalse(graph.HasCycle());
    });

    // =========================================================================
    // Topological sort
    // =========================================================================

    auto t19 = larvae::RegisterTest("NectarDepGraph", "TopologicalSortChain", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Hard);

        wax::Vector<nectar::AssetId> order{alloc};
        larvae::AssertTrue(graph.TopologicalSort(order));
        larvae::AssertEqual(order.Size(), size_t{3});

        // c must come before b, b before a (dependencies first)
        size_t pos_a = 0, pos_b = 0, pos_c = 0;
        for (size_t i = 0; i < order.Size(); ++i)
        {
            if (order[i] == a) pos_a = i;
            if (order[i] == b) pos_b = i;
            if (order[i] == c) pos_c = i;
        }
        larvae::AssertTrue(pos_c < pos_b);
        larvae::AssertTrue(pos_b < pos_a);
    });

    auto t20 = larvae::RegisterTest("NectarDepGraph", "TopologicalSortDiamond", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        auto d = MakeId(4);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(a, c, nectar::DepKind::Hard);
        graph.AddEdge(b, d, nectar::DepKind::Hard);
        graph.AddEdge(c, d, nectar::DepKind::Hard);

        wax::Vector<nectar::AssetId> order{alloc};
        larvae::AssertTrue(graph.TopologicalSort(order));
        larvae::AssertEqual(order.Size(), size_t{4});

        // d must be before b and c, b and c before a
        size_t pos_a = 0, pos_b = 0, pos_c = 0, pos_d = 0;
        for (size_t i = 0; i < order.Size(); ++i)
        {
            if (order[i] == a) pos_a = i;
            if (order[i] == b) pos_b = i;
            if (order[i] == c) pos_c = i;
            if (order[i] == d) pos_d = i;
        }
        larvae::AssertTrue(pos_d < pos_b);
        larvae::AssertTrue(pos_d < pos_c);
        larvae::AssertTrue(pos_b < pos_a);
        larvae::AssertTrue(pos_c < pos_a);
    });

    auto t21 = larvae::RegisterTest("NectarDepGraph", "TopologicalSortForest", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        auto d = MakeId(4);
        // Two independent chains: a->b and c->d
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(c, d, nectar::DepKind::Hard);

        wax::Vector<nectar::AssetId> order{alloc};
        larvae::AssertTrue(graph.TopologicalSort(order));
        larvae::AssertEqual(order.Size(), size_t{4});
    });

    auto t22 = larvae::RegisterTest("NectarDepGraph", "TopologicalSortEmpty", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        wax::Vector<nectar::AssetId> order{alloc};
        larvae::AssertTrue(graph.TopologicalSort(order));
        larvae::AssertEqual(order.Size(), size_t{0});
    });

    // =========================================================================
    // Stats
    // =========================================================================

    auto t23 = larvae::RegisterTest("NectarDepGraph", "HasNode", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        larvae::AssertTrue(graph.HasNode(a));
        larvae::AssertTrue(graph.HasNode(b));
        larvae::AssertFalse(graph.HasNode(MakeId(99)));
    });

    auto t24 = larvae::RegisterTest("NectarDepGraph", "MultipleEdgesBetweenNodes", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(a, c, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Soft);

        larvae::AssertEqual(graph.EdgeCount(), size_t{3});
        larvae::AssertEqual(graph.NodeCount(), size_t{3});
    });

    // =========================================================================
    // TopologicalSortLevels
    // =========================================================================

    auto t25b = larvae::RegisterTest("NectarDepGraph", "LevelsSingleNode", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        graph.AddEdge(a, b, nectar::DepKind::Hard);

        wax::Vector<wax::Vector<nectar::AssetId>> levels{alloc};
        larvae::AssertTrue(graph.TopologicalSortLevels(levels));
        larvae::AssertEqual(levels.Size(), size_t{2});
        // Level 0: b (no deps), Level 1: a (depends on b)
        larvae::AssertEqual(levels[0].Size(), size_t{1});
        larvae::AssertEqual(levels[1].Size(), size_t{1});
    });

    auto t25c = larvae::RegisterTest("NectarDepGraph", "LevelsLinearChain", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        // a -> b -> c
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Hard);

        wax::Vector<wax::Vector<nectar::AssetId>> levels{alloc};
        larvae::AssertTrue(graph.TopologicalSortLevels(levels));
        larvae::AssertEqual(levels.Size(), size_t{3});
        larvae::AssertEqual(levels[0].Size(), size_t{1}); // c
        larvae::AssertEqual(levels[1].Size(), size_t{1}); // b
        larvae::AssertEqual(levels[2].Size(), size_t{1}); // a
    });

    auto t25d = larvae::RegisterTest("NectarDepGraph", "LevelsDiamond", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        auto d = MakeId(4);
        // a -> b, a -> c, b -> d, c -> d (diamond)
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(a, c, nectar::DepKind::Hard);
        graph.AddEdge(b, d, nectar::DepKind::Hard);
        graph.AddEdge(c, d, nectar::DepKind::Hard);

        wax::Vector<wax::Vector<nectar::AssetId>> levels{alloc};
        larvae::AssertTrue(graph.TopologicalSortLevels(levels));
        larvae::AssertEqual(levels.Size(), size_t{3});
        // Level 0: d (no deps)
        larvae::AssertEqual(levels[0].Size(), size_t{1});
        // Level 1: b, c (both depend only on d)
        larvae::AssertEqual(levels[1].Size(), size_t{2});
        // Level 2: a (depends on b and c)
        larvae::AssertEqual(levels[2].Size(), size_t{1});
    });

    auto t25 = larvae::RegisterTest("NectarDepGraph", "RemoveNodeFromMiddle", []() {
        auto& alloc = GetGraphAlloc();
        nectar::DependencyGraph graph{alloc};
        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        graph.AddEdge(a, b, nectar::DepKind::Hard);
        graph.AddEdge(b, c, nectar::DepKind::Hard);

        graph.RemoveNode(b);
        // a and c still exist as isolated nodes, but b and its edges are gone
        larvae::AssertFalse(graph.HasEdge(a, b));
        larvae::AssertFalse(graph.HasEdge(b, c));
        larvae::AssertEqual(graph.EdgeCount(), size_t{0});
    });

}
