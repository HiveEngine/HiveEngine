
#include <larvae/larvae.h>
#include <queen/hierarchy/hierarchy.h>
#include <queen/world/world.h>

namespace
{
    // Test component
    struct Position
    {
        float x, y, z;
    };

    // ─────────────────────────────────────────────────────────────
    // Parent Component Tests
    // ─────────────────────────────────────────────────────────────

    auto test_hierarchy_1 = larvae::RegisterTest("QueenHierarchy", "ParentComponentConstruction", []() {
        queen::Parent p1;
        larvae::AssertFalse(p1.IsValid());
        larvae::AssertTrue(p1.entity.IsNull());

        queen::Entity e{42, 1};
        queen::Parent p2{e};
        larvae::AssertTrue(p2.IsValid());
        larvae::AssertEqual(p2.entity.Index(), 42u);
    });

    auto test_hierarchy_2 = larvae::RegisterTest("QueenHierarchy", "ParentComponentEquality", []() {
        queen::Entity e1{1, 1};
        queen::Entity e2{2, 1};

        queen::Parent p1{e1};
        queen::Parent p2{e1};
        queen::Parent p3{e2};

        larvae::AssertTrue(p1 == p2);
        larvae::AssertFalse(p1 == p3);
        larvae::AssertTrue(p1 != p3);
    });

    // ─────────────────────────────────────────────────────────────
    // Children Component Tests
    // ─────────────────────────────────────────────────────────────

    auto test_hierarchy_3 = larvae::RegisterTest("QueenHierarchy", "ChildrenComponentAddRemove", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::ChildrenT<comb::BuddyAllocator> children{alloc};

        larvae::AssertTrue(children.IsEmpty());
        larvae::AssertEqual(children.Count(), size_t{0});

        queen::Entity e1{1, 1};
        queen::Entity e2{2, 1};
        queen::Entity e3{3, 1};

        children.Add(e1);
        children.Add(e2);
        children.Add(e3);

        larvae::AssertFalse(children.IsEmpty());
        larvae::AssertEqual(children.Count(), size_t{3});
        larvae::AssertTrue(children.Contains(e1));
        larvae::AssertTrue(children.Contains(e2));
        larvae::AssertTrue(children.Contains(e3));

        bool removed = children.Remove(e2);
        larvae::AssertTrue(removed);
        larvae::AssertEqual(children.Count(), size_t{2});
        larvae::AssertFalse(children.Contains(e2));
        larvae::AssertTrue(children.Contains(e1));
        larvae::AssertTrue(children.Contains(e3));

        removed = children.Remove(queen::Entity{99, 1}); // Non-existent
        larvae::AssertFalse(removed);
    });

    auto test_hierarchy_4 = larvae::RegisterTest("QueenHierarchy", "ChildrenComponentIteration", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::ChildrenT<comb::BuddyAllocator> children{alloc};

        queen::Entity e1{1, 1};
        queen::Entity e2{2, 1};
        queen::Entity e3{3, 1};

        children.Add(e1);
        children.Add(e2);
        children.Add(e3);

        size_t count = 0;
        for (queen::Entity e : children)
        {
            (void)e;
            ++count;
        }

        larvae::AssertEqual(count, size_t{3});
    });

    // ─────────────────────────────────────────────────────────────
    // World Hierarchy Basic Tests
    // ─────────────────────────────────────────────────────────────

    auto test_hierarchy_5 = larvae::RegisterTest("QueenHierarchy", "SetParentBasic", []() {
        queen::World world;

        queen::Entity parent = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child = world.Spawn(Position{1.0f, 1.0f, 1.0f});

        larvae::AssertFalse(world.HasParent(child));

        world.SetParent(child, parent);

        larvae::AssertTrue(world.HasParent(child));
        larvae::AssertTrue(world.GetParent(child) == parent);
    });

    auto test_hierarchy_6 = larvae::RegisterTest("QueenHierarchy", "SetParentUpdatesChildren", []() {
        queen::World world;

        queen::Entity parent = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child1 = world.Spawn(Position{1.0f, 1.0f, 1.0f});
        queen::Entity child2 = world.Spawn(Position{2.0f, 2.0f, 2.0f});

        world.SetParent(child1, parent);
        world.SetParent(child2, parent);

        larvae::AssertEqual(world.ChildCount(parent), size_t{2});

        const queen::Children* children = world.GetChildren(parent);
        larvae::AssertNotNull(children);
        larvae::AssertTrue(children->Contains(child1));
        larvae::AssertTrue(children->Contains(child2));
    });

    auto test_hierarchy_7 = larvae::RegisterTest("QueenHierarchy", "RemoveParentBasic", []() {
        queen::World world;

        queen::Entity parent = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child = world.Spawn(Position{1.0f, 1.0f, 1.0f});

        world.SetParent(child, parent);
        larvae::AssertTrue(world.HasParent(child));

        world.RemoveParent(child);

        larvae::AssertFalse(world.HasParent(child));
        larvae::AssertTrue(world.GetParent(child).IsNull());
        larvae::AssertEqual(world.ChildCount(parent), size_t{0});
    });

    auto test_hierarchy_8 = larvae::RegisterTest("QueenHierarchy", "GetParentReturnsInvalidForNoParent", []() {
        queen::World world;

        queen::Entity entity = world.Spawn(Position{0.0f, 0.0f, 0.0f});

        queen::Entity parent = world.GetParent(entity);
        larvae::AssertTrue(parent.IsNull());
    });

    // ─────────────────────────────────────────────────────────────
    // Reparenting Tests
    // ─────────────────────────────────────────────────────────────

    auto test_hierarchy_9 = larvae::RegisterTest("QueenHierarchy", "ReparentRemovesFromOldParent", []() {
        queen::World world;

        queen::Entity parent1 = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity parent2 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity child = world.Spawn(Position{2.0f, 0.0f, 0.0f});

        world.SetParent(child, parent1);
        larvae::AssertEqual(world.ChildCount(parent1), size_t{1});
        larvae::AssertEqual(world.ChildCount(parent2), size_t{0});

        world.SetParent(child, parent2);

        larvae::AssertEqual(world.ChildCount(parent1), size_t{0});
        larvae::AssertEqual(world.ChildCount(parent2), size_t{1});
        larvae::AssertTrue(world.GetParent(child) == parent2);
    });

    // ─────────────────────────────────────────────────────────────
    // ForEachChild Tests
    // ─────────────────────────────────────────────────────────────

    auto test_hierarchy_10 = larvae::RegisterTest("QueenHierarchy", "ForEachChildIteratesAll", []() {
        queen::World world;

        queen::Entity parent = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity child2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity child3 = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        world.SetParent(child1, parent);
        world.SetParent(child2, parent);
        world.SetParent(child3, parent);

        size_t count = 0;
        world.ForEachChild(parent, [&count](queen::Entity) {
            ++count;
        });

        larvae::AssertEqual(count, size_t{3});
    });

    auto test_hierarchy_11 = larvae::RegisterTest("QueenHierarchy", "ForEachChildNoChildrenDoesNothing", []() {
        queen::World world;

        queen::Entity entity = world.Spawn(Position{0.0f, 0.0f, 0.0f});

        size_t count = 0;
        world.ForEachChild(entity, [&count](queen::Entity) {
            ++count;
        });

        larvae::AssertEqual(count, size_t{0});
    });

    // ─────────────────────────────────────────────────────────────
    // Hierarchy Traversal Tests
    // ─────────────────────────────────────────────────────────────

    auto test_hierarchy_12 = larvae::RegisterTest("QueenHierarchy", "ForEachDescendantIteratesAll", []() {
        queen::World world;

        // Create hierarchy:
        //       root
        //      /    \
        //   child1  child2
        //     |
        //  grandchild

        queen::Entity root = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity child2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity grandchild = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        world.SetParent(child1, root);
        world.SetParent(child2, root);
        world.SetParent(grandchild, child1);

        size_t count = 0;
        world.ForEachDescendant(root, [&count](queen::Entity) {
            ++count;
        });

        larvae::AssertEqual(count, size_t{3}); // child1, child2, grandchild
    });

    auto test_hierarchy_13 = larvae::RegisterTest("QueenHierarchy", "IsDescendantOfWorks", []() {
        queen::World world;

        queen::Entity root = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity grandchild = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity unrelated = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        world.SetParent(child, root);
        world.SetParent(grandchild, child);

        larvae::AssertTrue(world.IsDescendantOf(child, root));
        larvae::AssertTrue(world.IsDescendantOf(grandchild, root));
        larvae::AssertTrue(world.IsDescendantOf(grandchild, child));

        larvae::AssertFalse(world.IsDescendantOf(root, child));
        larvae::AssertFalse(world.IsDescendantOf(unrelated, root));
        larvae::AssertFalse(world.IsDescendantOf(root, root)); // Not descendant of self
    });

    auto test_hierarchy_14 = larvae::RegisterTest("QueenHierarchy", "GetRootFindsRoot", []() {
        queen::World world;

        queen::Entity root = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity grandchild = world.Spawn(Position{2.0f, 0.0f, 0.0f});

        world.SetParent(child, root);
        world.SetParent(grandchild, child);

        larvae::AssertTrue(world.GetRoot(root) == root);
        larvae::AssertTrue(world.GetRoot(child) == root);
        larvae::AssertTrue(world.GetRoot(grandchild) == root);
    });

    auto test_hierarchy_15 = larvae::RegisterTest("QueenHierarchy", "GetDepthCorrect", []() {
        queen::World world;

        queen::Entity root = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity grandchild = world.Spawn(Position{2.0f, 0.0f, 0.0f});

        world.SetParent(child, root);
        world.SetParent(grandchild, child);

        larvae::AssertEqual(world.GetDepth(root), 0u);
        larvae::AssertEqual(world.GetDepth(child), 1u);
        larvae::AssertEqual(world.GetDepth(grandchild), 2u);
    });

    // ─────────────────────────────────────────────────────────────
    // Despawn Tests
    // ─────────────────────────────────────────────────────────────

    auto test_hierarchy_16 = larvae::RegisterTest("QueenHierarchy", "DespawnChildRemovesFromParent", []() {
        queen::World world;

        queen::Entity parent = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        world.SetParent(child, parent);
        larvae::AssertEqual(world.ChildCount(parent), size_t{1});

        world.RemoveParent(child);
        world.Despawn(child);

        larvae::AssertEqual(world.ChildCount(parent), size_t{0});
        larvae::AssertFalse(world.IsAlive(child));
    });

    auto test_hierarchy_17 = larvae::RegisterTest("QueenHierarchy", "DespawnRecursiveDespawnsAll", []() {
        queen::World world;

        //       root
        //      /    \
        //   child1  child2
        //     |
        //  grandchild

        queen::Entity root = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity child2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity grandchild = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        world.SetParent(child1, root);
        world.SetParent(child2, root);
        world.SetParent(grandchild, child1);

        larvae::AssertEqual(world.EntityCount(), size_t{4});

        world.DespawnRecursive(root);

        larvae::AssertEqual(world.EntityCount(), size_t{0});
        larvae::AssertFalse(world.IsAlive(root));
        larvae::AssertFalse(world.IsAlive(child1));
        larvae::AssertFalse(world.IsAlive(child2));
        larvae::AssertFalse(world.IsAlive(grandchild));
    });

    auto test_hierarchy_18 = larvae::RegisterTest("QueenHierarchy", "DespawnRecursiveSubtree", []() {
        queen::World world;

        //       root
        //      /    \
        //   child1  child2
        //     |
        //  grandchild

        queen::Entity root = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity child2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity grandchild = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        world.SetParent(child1, root);
        world.SetParent(child2, root);
        world.SetParent(grandchild, child1);

        // Despawn only child1 subtree
        world.DespawnRecursive(child1);

        larvae::AssertTrue(world.IsAlive(root));
        larvae::AssertTrue(world.IsAlive(child2));
        larvae::AssertFalse(world.IsAlive(child1));
        larvae::AssertFalse(world.IsAlive(grandchild));

        larvae::AssertEqual(world.ChildCount(root), size_t{1});
    });

    // ─────────────────────────────────────────────────────────────
    // Edge Cases
    // ─────────────────────────────────────────────────────────────

    auto test_hierarchy_19 = larvae::RegisterTest("QueenHierarchy", "DeepHierarchyTraversal", []() {
        queen::World world;

        constexpr size_t depth = 100;
        queen::Entity entities[depth];

        entities[0] = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        for (size_t i = 1; i < depth; ++i)
        {
            entities[i] = world.Spawn(Position{static_cast<float>(i), 0.0f, 0.0f});
            world.SetParent(entities[i], entities[i - 1]);
        }

        // Check depth
        larvae::AssertEqual(world.GetDepth(entities[depth - 1]), static_cast<uint32_t>(depth - 1));

        // Check root
        larvae::AssertTrue(world.GetRoot(entities[depth - 1]) == entities[0]);

        // Check descendant count
        size_t count = 0;
        world.ForEachDescendant(entities[0], [&count](queen::Entity) {
            ++count;
        });
        larvae::AssertEqual(count, depth - 1);
    });

    auto test_hierarchy_20 = larvae::RegisterTest("QueenHierarchy", "ManyChildrenPerformance", []() {
        queen::World world;

        queen::Entity parent = world.Spawn(Position{0.0f, 0.0f, 0.0f});

        constexpr size_t child_count = 1000;
        for (size_t i = 0; i < child_count; ++i)
        {
            queen::Entity child = world.Spawn(Position{static_cast<float>(i), 0.0f, 0.0f});
            world.SetParent(child, parent);
        }

        larvae::AssertEqual(world.ChildCount(parent), child_count);

        // Iterate all children
        size_t count = 0;
        world.ForEachChild(parent, [&count](queen::Entity) {
            ++count;
        });
        larvae::AssertEqual(count, child_count);
    });

    auto test_hierarchy_21 = larvae::RegisterTest("QueenHierarchy", "RemoveParentFromEntityWithNoParent", []() {
        queen::World world;

        queen::Entity entity = world.Spawn(Position{0.0f, 0.0f, 0.0f});

        // Should not crash
        world.RemoveParent(entity);

        larvae::AssertFalse(world.HasParent(entity));
    });

    auto test_hierarchy_22 = larvae::RegisterTest("QueenHierarchy", "QueryWithParentComponent", []() {
        queen::World world;

        queen::Entity root = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity child2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity unparented = world.Spawn(Position{3.0f, 0.0f, 0.0f});
        (void)unparented;

        world.SetParent(child1, root);
        world.SetParent(child2, root);

        // Query entities that have parents
        size_t count = 0;
        world.Query<queen::Read<queen::Parent>, queen::Read<Position>>()
            .Each([&count](const queen::Parent& p, const Position&) {
                (void)p;
                ++count;
            });

        larvae::AssertEqual(count, size_t{2}); // child1 and child2
    });

    auto test_hierarchy_23 = larvae::RegisterTest("QueenHierarchy", "DespawnRecursiveOnLeaf", []() {
        queen::World world;

        queen::Entity parent = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity child = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        world.SetParent(child, parent);

        // DespawnRecursive on a leaf (no children) should just despawn it
        world.DespawnRecursive(child);

        larvae::AssertTrue(world.IsAlive(parent));
        larvae::AssertFalse(world.IsAlive(child));
        larvae::AssertEqual(world.ChildCount(parent), size_t{0});
    });
}
