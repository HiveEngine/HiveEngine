#include <hive/math/math.h>

#include <queen/world/world.h>

#include <waggle/components/transform.h>
#include <waggle/systems/transform_system.h>
#include <waggle/time.h>

#include <larvae/larvae.h>

#include <cmath>

namespace
{
    using namespace hive::math;

    void SetupWorld(queen::World& world)
    {
        world.InsertResource(waggle::Time{1.f / 60.f, 0.f, 16666666, 0, 1});
        waggle::RegisterTransformObservers(world);
    }

    bool Near(float a, float b, float eps = 1e-4f)
    {
        return std::fabs(a - b) < eps;
    }

    auto t_root_identity = larvae::RegisterTest("TransformSystem", "RootIdentityWorldMatrix", []() {
        queen::World world;
        SetupWorld(world);

        queen::Entity e = world.Spawn(
            waggle::Transform{},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        waggle::TransformSystem(world);

        auto* wm = world.Get<waggle::WorldMatrix>(e);
        larvae::AssertTrue(wm != nullptr);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 0.f));
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][1], 0.f));
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][2], 0.f));
    });

    auto t_root_translation = larvae::RegisterTest("TransformSystem", "RootTranslationPropagates", []() {
        queen::World world;
        SetupWorld(world);

        queen::Entity e = world.Spawn(
            waggle::Transform{{5.f, 3.f, -2.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        waggle::TransformSystem(world);

        auto* wm = world.Get<waggle::WorldMatrix>(e);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 5.f));
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][1], 3.f));
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][2], -2.f));
    });

    auto t_child_inherits_parent = larvae::RegisterTest("TransformSystem", "ChildInheritsParentTranslation", []() {
        queen::World world;
        SetupWorld(world);

        queen::Entity parent = world.Spawn(
            waggle::Transform{{10.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        queen::Entity child = world.Spawn(
            waggle::Transform{{0.f, 5.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        world.SetParent(child, parent);
        waggle::TransformSystem(world);

        auto* wm = world.Get<waggle::WorldMatrix>(child);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 10.f));
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][1], 5.f));
    });

    auto t_deep_hierarchy = larvae::RegisterTest("TransformSystem", "DeepHierarchyPropagates", []() {
        queen::World world;
        SetupWorld(world);

        queen::Entity a = world.Spawn(
            waggle::Transform{{1.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        queen::Entity b = world.Spawn(
            waggle::Transform{{2.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        queen::Entity c = world.Spawn(
            waggle::Transform{{3.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        world.SetParent(b, a);
        world.SetParent(c, b);
        waggle::TransformSystem(world);

        auto* wmC = world.Get<waggle::WorldMatrix>(c);
        // A(1) + B(2) + C(3) = 6
        larvae::AssertTrue(Near(wmC->m_matrix.m_m[3][0], 6.f));
    });

    auto t_parent_scale_affects_child = larvae::RegisterTest("TransformSystem", "ParentScaleAffectsChild", []() {
        queen::World world;
        SetupWorld(world);

        queen::Entity parent = world.Spawn(
            waggle::Transform{{0.f, 0.f, 0.f}, {}, {2.f, 2.f, 2.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        queen::Entity child = world.Spawn(
            waggle::Transform{{5.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        world.SetParent(child, parent);
        waggle::TransformSystem(world);

        auto* wm = world.Get<waggle::WorldMatrix>(child);
        // Parent scale 2x → child at local (5,0,0) → world (10,0,0)
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 10.f));
    });

    auto t_dirty_only = larvae::RegisterTest("TransformSystem", "OnlyDirtyEntitiesRecompute", []() {
        queen::World world;
        SetupWorld(world);

        queen::Entity e = world.Spawn(
            waggle::Transform{{1.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        waggle::TransformSystem(world);

        auto* wm = world.Get<waggle::WorldMatrix>(e);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 1.f));

        // Manually change transform without marking dirty
        auto* tf = world.Get<waggle::Transform>(e);
        tf->m_position.m_x = 99.f;

        // Tick 2 — version still says tick 1, should NOT recompute
        world.Resource<waggle::Time>()->m_tick = 2;
        waggle::TransformSystem(world);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 1.f));

        // Mark dirty at tick 2
        world.Get<waggle::TransformVersion>(e)->m_lastModified = 2;
        waggle::TransformSystem(world);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 99.f));
    });

    auto t_reparent_updates = larvae::RegisterTest("TransformSystem", "ReparentUpdatesWorldMatrix", []() {
        queen::World world;
        SetupWorld(world);

        queen::Entity a = world.Spawn(
            waggle::Transform{{10.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        queen::Entity b = world.Spawn(
            waggle::Transform{{20.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        queen::Entity child = world.Spawn(
            waggle::Transform{{1.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        world.SetParent(child, a);
        waggle::TransformSystem(world);

        auto* wm = world.Get<waggle::WorldMatrix>(child);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 11.f));

        // Reparent to b at tick 2
        world.Resource<waggle::Time>()->m_tick = 2;
        world.Get<waggle::TransformVersion>(b)->m_lastModified = 2;
        world.SetParent(child, b);
        waggle::TransformSystem(world);

        wm = world.Get<waggle::WorldMatrix>(child);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 21.f));
    });
    auto t_deep_chain_100 = larvae::RegisterTest("TransformSystem", "DeepChain100Levels", []() {
        queen::World world;
        SetupWorld(world);

        constexpr int kDepth = 100;
        queen::Entity prev{};
        for (int i = 0; i < kDepth; ++i)
        {
            queen::Entity e = world.Spawn(
                waggle::Transform{{1.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
                waggle::WorldMatrix{},
                waggle::TransformVersion{1});
            if (world.IsAlive(prev))
                world.SetParent(e, prev);
            prev = e;
        }

        waggle::TransformSystem(world);

        auto* wm = world.Get<waggle::WorldMatrix>(prev);
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], static_cast<float>(kDepth)));
    });

    auto t_wide_tree_1000 = larvae::RegisterTest("TransformSystem", "WideTree1000Children", []() {
        queen::World world;
        SetupWorld(world);

        queen::Entity root = world.Spawn(
            waggle::Transform{{5.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
            waggle::WorldMatrix{},
            waggle::TransformVersion{1});

        constexpr int kChildren = 1000;
        queen::Entity lastChild{};
        for (int i = 0; i < kChildren; ++i)
        {
            queen::Entity child = world.Spawn(
                waggle::Transform{{static_cast<float>(i), 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
                waggle::WorldMatrix{},
                waggle::TransformVersion{1});
            world.SetParent(child, root);
            lastChild = child;
        }

        waggle::TransformSystem(world);

        auto* wm = world.Get<waggle::WorldMatrix>(lastChild);
        // root(5) + lastChild(999) = 1004
        larvae::AssertTrue(Near(wm->m_matrix.m_m[3][0], 1004.f));
    });

    auto t_mixed_10k = larvae::RegisterTest("TransformSystem", "MixedHierarchy10kEntities", []() {
        queen::World world;
        SetupWorld(world);

        constexpr int kRoots = 100;
        constexpr int kChildrenPerRoot = 100;

        for (int r = 0; r < kRoots; ++r)
        {
            queen::Entity root = world.Spawn(
                waggle::Transform{{static_cast<float>(r), 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
                waggle::WorldMatrix{},
                waggle::TransformVersion{1});

            for (int c = 0; c < kChildrenPerRoot; ++c)
            {
                queen::Entity child = world.Spawn(
                    waggle::Transform{{1.f, 0.f, 0.f}, {}, {1.f, 1.f, 1.f}},
                    waggle::WorldMatrix{},
                    waggle::TransformVersion{1});
                world.SetParent(child, root);
            }
        }

        waggle::TransformSystem(world);

        // Spot check: first root at (0,0,0), its children at (1,0,0)
        // Last root at (99,0,0), its children at (100,0,0)
        queen::Entity lastRoot{};
        queen::Entity lastChild{};
        world.ForEachArchetype([&](auto& arch) {
            if (arch.template HasComponent<queen::Parent>())
                return;
            if (!arch.template HasComponent<waggle::Transform>())
                return;
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
                lastRoot = arch.GetEntity(row);
        });
        world.ForEachChild(lastRoot, [&](queen::Entity child) { lastChild = child; });

        auto* rootWm = world.Get<waggle::WorldMatrix>(lastRoot);
        auto* childWm = world.Get<waggle::WorldMatrix>(lastChild);
        larvae::AssertTrue(rootWm != nullptr);
        larvae::AssertTrue(childWm != nullptr);
        float rootX = rootWm->m_matrix.m_m[3][0];
        float childX = childWm->m_matrix.m_m[3][0];
        // Child should be 1 unit ahead of its root
        larvae::AssertTrue(Near(childX - rootX, 1.f));
    });
} // namespace
