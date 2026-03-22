#include <queen/world/world.h>

#include <waggle/components/disabled.h>
#include <waggle/disabled_propagation.h>

#include <larvae/larvae.h>

namespace
{
    auto t_no_disabled_by_default = larvae::RegisterTest("DisabledPropagation", "NoTagsByDefault", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto e = world.Spawn().Build();

        larvae::AssertFalse(world.Has<waggle::Disabled>(e));
        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(e));
    });

    auto t_disable_adds_hd = larvae::RegisterTest("DisabledPropagation", "DisableAddsHierarchyDisabled", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto e = world.Spawn().Build();

        waggle::SetEntityDisabled(world, e, true);

        larvae::AssertTrue(world.Has<waggle::Disabled>(e));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(e));
    });

    auto t_enable_removes_hd = larvae::RegisterTest("DisabledPropagation", "EnableRemovesHierarchyDisabled", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto e = world.Spawn().Build();

        waggle::SetEntityDisabled(world, e, true);
        waggle::SetEntityDisabled(world, e, false);

        larvae::AssertFalse(world.Has<waggle::Disabled>(e));
        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(e));
    });

    auto t_child_inherits = larvae::RegisterTest("DisabledPropagation", "ChildInheritsFromParent", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto parent = world.Spawn().Build();
        auto child = world.Spawn().Build();
        world.SetParent(child, parent);

        waggle::SetEntityDisabled(world, parent, true);

        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(parent));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));
        larvae::AssertFalse(world.Has<waggle::Disabled>(child));
    });

    auto t_deep_hierarchy = larvae::RegisterTest("DisabledPropagation", "DeepHierarchyPropagation", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto a = world.Spawn().Build();
        auto b = world.Spawn().Build();
        auto c = world.Spawn().Build();
        auto d = world.Spawn().Build();
        world.SetParent(b, a);
        world.SetParent(c, b);
        world.SetParent(d, c);

        waggle::SetEntityDisabled(world, a, true);

        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(a));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(b));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(c));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(d));
    });

    auto t_enable_parent_clears_children =
        larvae::RegisterTest("DisabledPropagation", "EnableParentClearsChildren", []() {
            queen::World world{};
            waggle::RegisterDisabledObservers(world);
            auto parent = world.Spawn().Build();
            auto child = world.Spawn().Build();
            world.SetParent(child, parent);

            waggle::SetEntityDisabled(world, parent, true);
            waggle::SetEntityDisabled(world, parent, false);

            larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(parent));
            larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(child));
        });

    auto t_child_disabled_independently =
        larvae::RegisterTest("DisabledPropagation", "ChildDisabledIndependently", []() {
            queen::World world{};
            waggle::RegisterDisabledObservers(world);
            auto parent = world.Spawn().Build();
            auto child = world.Spawn().Build();
            world.SetParent(child, parent);

            waggle::SetEntityDisabled(world, child, true);

            larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(parent));
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));
        });

    auto t_enable_parent_keeps_child_disabled =
        larvae::RegisterTest("DisabledPropagation", "EnableParentKeepsExplicitlyDisabledChild", []() {
            queen::World world{};
            waggle::RegisterDisabledObservers(world);
            auto parent = world.Spawn().Build();
            auto child = world.Spawn().Build();
            world.SetParent(child, parent);

            waggle::SetEntityDisabled(world, parent, true);
            waggle::SetEntityDisabled(world, child, true);
            waggle::SetEntityDisabled(world, parent, false);

            larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(parent));
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));
            larvae::AssertTrue(world.Has<waggle::Disabled>(child));
        });

    auto t_multiple_children = larvae::RegisterTest("DisabledPropagation", "MultipleChildrenAllInherit", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto parent = world.Spawn().Build();
        auto c1 = world.Spawn().Build();
        auto c2 = world.Spawn().Build();
        auto c3 = world.Spawn().Build();
        world.SetParent(c1, parent);
        world.SetParent(c2, parent);
        world.SetParent(c3, parent);

        waggle::SetEntityDisabled(world, parent, true);

        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(c1));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(c2));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(c3));
    });

    auto t_sibling_unaffected = larvae::RegisterTest("DisabledPropagation", "SiblingUnaffected", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto parent = world.Spawn().Build();
        auto child1 = world.Spawn().Build();
        auto child2 = world.Spawn().Build();
        world.SetParent(child1, parent);
        world.SetParent(child2, parent);

        waggle::SetEntityDisabled(world, child1, true);

        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child1));
        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(child2));
        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(parent));
    });

    auto t_reparent_under_disabled = larvae::RegisterTest("DisabledPropagation", "ReparentUnderDisabledParent", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto parent = world.Spawn().Build();
        auto child = world.Spawn().Build();

        waggle::SetEntityDisabled(world, parent, true);
        world.SetParent(child, parent);

        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));
    });

    auto t_reparent_away_from_disabled =
        larvae::RegisterTest("DisabledPropagation", "ReparentAwayFromDisabledParent", []() {
            queen::World world{};
            waggle::RegisterDisabledObservers(world);
            auto disabledParent = world.Spawn().Build();
            auto enabledParent = world.Spawn().Build();
            auto child = world.Spawn().Build();

            waggle::SetEntityDisabled(world, disabledParent, true);
            world.SetParent(child, disabledParent);
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));

            world.RemoveParent(child);
            waggle::RecalculateEntityHierarchyDisabled(world, child);
            world.SetParent(child, enabledParent);

            larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(child));
        });

    auto t_detach_from_disabled = larvae::RegisterTest("DisabledPropagation", "DetachFromDisabledParent", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto parent = world.Spawn().Build();
        auto child = world.Spawn().Build();
        world.SetParent(child, parent);

        waggle::SetEntityDisabled(world, parent, true);
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));

        world.RemoveParent(child);
        waggle::RecalculateEntityHierarchyDisabled(world, child);

        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(child));
    });

    auto t_detach_keeps_own_disabled = larvae::RegisterTest("DisabledPropagation", "DetachKeepsOwnDisabledTag", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto parent = world.Spawn().Build();
        auto child = world.Spawn().Build();
        world.SetParent(child, parent);

        waggle::SetEntityDisabled(world, parent, true);
        waggle::SetEntityDisabled(world, child, true);
        world.RemoveParent(child);
        waggle::RecalculateEntityHierarchyDisabled(world, child);

        larvae::AssertTrue(world.Has<waggle::Disabled>(child));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));
    });

    auto t_reparent_subtree_under_disabled =
        larvae::RegisterTest("DisabledPropagation", "ReparentSubtreeUnderDisabled", []() {
            queen::World world{};
            waggle::RegisterDisabledObservers(world);
            auto disabledRoot = world.Spawn().Build();
            auto subtreeRoot = world.Spawn().Build();
            auto subtreeChild = world.Spawn().Build();
            world.SetParent(subtreeChild, subtreeRoot);

            waggle::SetEntityDisabled(world, disabledRoot, true);
            world.SetParent(subtreeRoot, disabledRoot);

            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(subtreeRoot));
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(subtreeChild));
        });

    auto t_disable_middle_of_chain = larvae::RegisterTest("DisabledPropagation", "DisableMiddleOfChain", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto a = world.Spawn().Build();
        auto b = world.Spawn().Build();
        auto c = world.Spawn().Build();
        world.SetParent(b, a);
        world.SetParent(c, b);

        waggle::SetEntityDisabled(world, b, true);

        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(a));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(b));
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(c));
    });

    auto t_enable_middle_keeps_grandparent_disabled =
        larvae::RegisterTest("DisabledPropagation", "EnableMiddleWhenGrandparentDisabled", []() {
            queen::World world{};
            waggle::RegisterDisabledObservers(world);
            auto a = world.Spawn().Build();
            auto b = world.Spawn().Build();
            auto c = world.Spawn().Build();
            world.SetParent(b, a);
            world.SetParent(c, b);

            waggle::SetEntityDisabled(world, a, true);
            waggle::SetEntityDisabled(world, b, true);

            waggle::SetEntityDisabled(world, b, false);

            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(a));
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(b));
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(c));
        });

    auto t_root_entity_no_parent = larvae::RegisterTest("DisabledPropagation", "RootEntityNoParent", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto root = world.Spawn().Build();

        waggle::SetEntityDisabled(world, root, true);
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(root));

        waggle::SetEntityDisabled(world, root, false);
        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(root));
    });

    auto t_disable_enable_disable = larvae::RegisterTest("DisabledPropagation", "DisableEnableDisableCycle", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto parent = world.Spawn().Build();
        auto child = world.Spawn().Build();
        world.SetParent(child, parent);

        waggle::SetEntityDisabled(world, parent, true);
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));

        waggle::SetEntityDisabled(world, parent, false);
        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(child));

        waggle::SetEntityDisabled(world, parent, true);
        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));
    });

    auto t_wide_tree = larvae::RegisterTest("DisabledPropagation", "WideTreeAllChildrenDisabled", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto root = world.Spawn().Build();

        constexpr int COUNT = 50;
        queen::Entity children[COUNT];
        for (int i = 0; i < COUNT; ++i)
        {
            children[i] = world.Spawn().Build();
            world.SetParent(children[i], root);
        }

        waggle::SetEntityDisabled(world, root, true);

        for (int i = 0; i < COUNT; ++i)
        {
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(children[i]));
        }

        waggle::SetEntityDisabled(world, root, false);

        for (int i = 0; i < COUNT; ++i)
        {
            larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(children[i]));
        }
    });

    auto t_double_disable_idempotent = larvae::RegisterTest("DisabledPropagation", "DoubleDisableIsIdempotent", []() {
        queen::World world{};
        waggle::RegisterDisabledObservers(world);
        auto e = world.Spawn().Build();

        waggle::SetEntityDisabled(world, e, true);
        waggle::SetEntityDisabled(world, e, true);

        larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(e));

        waggle::SetEntityDisabled(world, e, false);
        larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(e));
    });

    auto t_reparent_between_disabled_parents =
        larvae::RegisterTest("DisabledPropagation", "ReparentBetweenDisabledParents", []() {
            queen::World world{};
            waggle::RegisterDisabledObservers(world);
            auto p1 = world.Spawn().Build();
            auto p2 = world.Spawn().Build();
            auto child = world.Spawn().Build();

            waggle::SetEntityDisabled(world, p1, true);
            waggle::SetEntityDisabled(world, p2, true);
            world.SetParent(child, p1);
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));

            world.RemoveParent(child);
            waggle::RecalculateEntityHierarchyDisabled(world, child);
            world.SetParent(child, p2);
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));
        });

    auto t_reparent_from_disabled_to_enabled =
        larvae::RegisterTest("DisabledPropagation", "ReparentFromDisabledToEnabled", []() {
            queen::World world{};
            waggle::RegisterDisabledObservers(world);
            auto disabled = world.Spawn().Build();
            auto enabled = world.Spawn().Build();
            auto child = world.Spawn().Build();
            auto grandchild = world.Spawn().Build();

            world.SetParent(child, disabled);
            world.SetParent(grandchild, child);
            waggle::SetEntityDisabled(world, disabled, true);

            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(child));
            larvae::AssertTrue(world.Has<waggle::HierarchyDisabled>(grandchild));

            world.RemoveParent(child);
            waggle::RecalculateEntityHierarchyDisabled(world, child);
            world.SetParent(child, enabled);

            larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(child));
            larvae::AssertFalse(world.Has<waggle::HierarchyDisabled>(grandchild));
        });
} // namespace
