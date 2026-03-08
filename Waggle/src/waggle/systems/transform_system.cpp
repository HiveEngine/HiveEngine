#include <hive/math/math.h>

#include <queen/hierarchy/parent.h>
#include <queen/query/query_term.h>
#include <queen/world/world.h>

#include <waggle/components/transform.h>
#include <waggle/systems/transform_system.h>
#include <waggle/time.h>

using namespace hive::math;

namespace waggle
{

    void TransformSystem(queen::World& world) {
        auto* time = world.Resource<Time>();
        if (!time)
            return;

        uint32_t tick = static_cast<uint32_t>(time->m_tick);

        // Pass 1 — Roots (no Parent): recompute if dirty or first frame
        world
            .Query<queen::Write<WorldMatrix>, queen::Read<Transform>, queen::Read<TransformVersion>,
                   queen::Without<queen::Parent>>()
            .Each([&](WorldMatrix& wm, const Transform& tf, const TransformVersion& ver) {
                if (ver.m_lastModified == tick || tick <= 1)
                    wm.m_matrix = TRS(tf.m_position, tf.m_rotation, tf.m_scale);
            });

        // Pass 2 — Children with Parent: recompute if self or parent dirty
        world
            .Query<queen::Write<WorldMatrix>, queen::Read<Transform>, queen::Read<TransformVersion>,
                   queen::Read<queen::Parent>>()
            .Each([&](WorldMatrix& wm, const Transform& tf, const TransformVersion& ver, const queen::Parent& parent) {
                bool selfDirty = (ver.m_lastModified == tick || tick <= 1);
                bool parentDirty = false;
                if (parent.IsValid())
                {
                    const auto* pver = world.Get<TransformVersion>(parent.m_entity);
                    if (pver && (pver->m_lastModified == tick || tick <= 1))
                        parentDirty = true;
                }

                if (selfDirty || parentDirty)
                {
                    Mat4 local = TRS(tf.m_position, tf.m_rotation, tf.m_scale);
                    if (parent.IsValid())
                    {
                        const auto* pwm = world.Get<WorldMatrix>(parent.m_entity);
                        wm.m_matrix = pwm ? pwm->m_matrix * local : local;
                    }
                    else
                    {
                        wm.m_matrix = local;
                    }
                }
            });
    }

    void WorldAABBSystem(queen::World& world) {
        auto* time = world.Resource<Time>();
        if (!time)
            return;

        uint32_t tick = static_cast<uint32_t>(time->m_tick);

        world
            .Query<queen::Write<WorldAABB>, queen::Read<WorldMatrix>, queen::Read<LocalAABB>,
                   queen::Read<TransformVersion>>()
            .Each([&](WorldAABB& waabb, const WorldMatrix& wm, const LocalAABB& local, const TransformVersion& ver) {
                if (ver.m_lastModified == tick || tick <= 1)
                    waabb.m_bounds = TransformAABB(wm.m_matrix, local.m_bounds);
            });
    }

} // namespace waggle
