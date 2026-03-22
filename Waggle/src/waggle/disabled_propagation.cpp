#include <queen/hierarchy/parent.h>
#include <queen/observer/observer_event.h>
#include <queen/world/world.h>

#include <waggle/components/disabled.h>
#include <waggle/disabled_propagation.h>

namespace waggle
{
    namespace
    {
        void PropagateDisabled(queen::World& world, queen::Entity entity, bool ancestorDisabled)
        {
            bool shouldBeHD = ancestorDisabled || world.Has<Disabled>(entity);

            if (shouldBeHD && !world.Has<HierarchyDisabled>(entity))
            {
                world.Set(entity, HierarchyDisabled{});
            }
            else if (!shouldBeHD && world.Has<HierarchyDisabled>(entity))
            {
                world.Remove<HierarchyDisabled>(entity);
            }

            world.ForEachChild(entity, [&](queen::Entity child) { PropagateDisabled(world, child, shouldBeHD); });
        }

        void RecalculateSubtree(queen::World& world, queen::Entity root)
        {
            bool parentHD = false;
            const auto* parent = world.Get<queen::Parent>(root);
            if (parent != nullptr)
            {
                parentHD = world.Has<HierarchyDisabled>(parent->m_entity);
            }

            PropagateDisabled(world, root, parentHD);
        }
    } // namespace

    void RegisterDisabledObservers(queen::World& world)
    {
        world.Observer<queen::OnAdd<Disabled>>("PropagateDisabledOnAdd")
            .EachWithWorld([](queen::World& w, queen::Entity e, const Disabled&) { RecalculateSubtree(w, e); });

        world.Observer<queen::OnAdd<queen::Parent>>("PropagateDisabledOnReparent")
            .EachWithWorld([](queen::World& w, queen::Entity e, const queen::Parent&) { RecalculateSubtree(w, e); });

        world.Observer<queen::OnSet<queen::Parent>>("PropagateDisabledOnSetParent")
            .EachWithWorld([](queen::World& w, queen::Entity e, const queen::Parent&) { RecalculateSubtree(w, e); });

        // OnRemove<Parent> not observed: Remove fires before the component is removed,
        // and modifying archetypes inside causes reentrancy issues.
        // Use RecalculateEntityHierarchyDisabled() after RemoveParent instead.
    }

    void RecalculateEntityHierarchyDisabled(queen::World& world, queen::Entity entity)
    {
        RecalculateSubtree(world, entity);
    }

    void SetEntityDisabled(queen::World& world, queen::Entity entity, bool disabled)
    {
        if (disabled)
        {
            world.Set(entity, Disabled{});
        }
        else
        {
            world.Remove<Disabled>(entity);
            RecalculateSubtree(world, entity);
        }
    }
} // namespace waggle
