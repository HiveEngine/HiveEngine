#include <waggle/systems/transform_system.h>

#include <hive/math/math.h>

#include <wax/containers/vector.h>

#include <queen/hierarchy/parent.h>
#include <queen/query/query_term.h>
#include <queen/world/world.h>

#include <waggle/components/transform.h>
#include <waggle/time.h>

using namespace hive::math;

namespace waggle
{
    void TransformSystem(queen::World& world)
    {
        auto* time = world.Resource<Time>();
        if (time == nullptr)
            return;

        uint64_t tick = time->m_tick;

        // Build BFS traversal order: roots first, then children layer by layer.
        // This guarantees every parent is processed before its children,
        // enabling a single linear pass for correct propagation at any depth.
        wax::Vector<queen::Entity> order{world.GetFrameAllocator()};

        world.ForEachArchetype([&](auto& arch) {
            if (arch.template HasComponent<queen::Parent>())
                return;
            if (!arch.template HasComponent<Transform>())
                return;
            if (!arch.template HasComponent<WorldMatrix>())
                return;
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
                order.PushBack(arch.GetEntity(row));
        });

        for (size_t i = 0; i < order.Size(); ++i)
        {
            world.ForEachChild(order[i], [&](queen::Entity child) {
                if (world.Has<Transform>(child) && world.Has<WorldMatrix>(child))
                    order.PushBack(child);
            });
        }

        // Linear pass — parents already computed when children are reached
        for (size_t i = 0; i < order.Size(); ++i)
        {
            queen::Entity e = order[i];
            auto* tf = world.Get<Transform>(e);
            auto* wm = world.Get<WorldMatrix>(e);
            auto* ver = world.Get<TransformVersion>(e);
            if (tf == nullptr || wm == nullptr)
                continue;

            bool dirty = (ver != nullptr && ver->m_lastModified == tick) || tick <= 1;

            queen::Entity parent = world.GetParent(e);
            if (world.IsAlive(parent))
            {
                auto* pver = world.Get<TransformVersion>(parent);
                if (pver != nullptr && pver->m_lastModified == tick)
                    dirty = true;

                if (dirty)
                {
                    auto* pwm = world.Get<WorldMatrix>(parent);
                    Mat4 local = TRS(tf->m_position, tf->m_rotation, tf->m_scale);
                    wm->m_matrix = pwm != nullptr ? pwm->m_matrix * local : local;
                    if (ver != nullptr)
                        ver->m_lastModified = tick;
                }
            }
            else
            {
                if (dirty)
                {
                    wm->m_matrix = TRS(tf->m_position, tf->m_rotation, tf->m_scale);
                }
            }
        }
    }

    void WorldAABBSystem(queen::World& world)
    {
        auto* time = world.Resource<Time>();
        if (time == nullptr)
            return;

        uint64_t tick = time->m_tick;

        world
            .Query<queen::Write<WorldAABB>, queen::Read<WorldMatrix>,
                   queen::Read<LocalAABB>, queen::Read<TransformVersion>>()
            .Each([&](WorldAABB& waabb, const WorldMatrix& wm,
                       const LocalAABB& local, const TransformVersion& ver) {
                if (ver.m_lastModified == tick || tick <= 1)
                    waabb.m_bounds = TransformAABB(wm.m_matrix, local.m_bounds);
            });
    }

    void RegisterTransformObservers(queen::World& world)
    {
        world.Observer<queen::OnSet<Transform>>("MarkTransformDirty")
            .EachWithWorld([](queen::World& w, queen::Entity entity, const Transform&) {
                auto* ver = w.Get<TransformVersion>(entity);
                auto* time = w.Resource<Time>();
                if (ver != nullptr && time != nullptr)
                    ver->m_lastModified = time->m_tick;
            });
    }

    void RegisterEngineSystems(queen::World& world)
    {
        world
            .System<queen::Read<Transform>, queen::Write<WorldMatrix>,
                    queen::Read<TransformVersion>, queen::Read<queen::Parent>>(
                "Engine.Transform")
            .WithResource<Time>()
            .Run(TransformSystem);

        world
            .System<queen::Read<WorldMatrix>, queen::Write<WorldAABB>,
                    queen::Read<LocalAABB>, queen::Read<TransformVersion>>(
                "Engine.WorldAABB")
            .WithResource<Time>()
            .Run(WorldAABBSystem);
    }

} // namespace waggle
