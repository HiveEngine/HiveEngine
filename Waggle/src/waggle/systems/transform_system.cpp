#include <waggle/systems/transform_system.h>
#include <waggle/components/transform.h>
#include <waggle/time.h>

#include <queen/world/world.h>
#include <queen/query/query_term.h>
#include <queen/hierarchy/parent.h>
#include <hive/math/math.h>

using namespace hive::math;

namespace waggle
{

void TransformSystem(queen::World& world)
{
    auto* time = world.Resource<Time>();
    if (!time) return;

    uint32_t tick = static_cast<uint32_t>(time->tick);

    // Pass 1 — Roots (no Parent): recompute if dirty or first frame
    world.Query<queen::Write<WorldMatrix>, queen::Read<Transform>,
                queen::Read<TransformVersion>, queen::Without<queen::Parent>>()
        .Each([&](WorldMatrix& wm, const Transform& tf, const TransformVersion& ver) {
            if (ver.last_modified == tick || tick <= 1)
                wm.matrix = TRS(tf.position, tf.rotation, tf.scale);
        });

    // Pass 2 — Children with Parent: recompute if self or parent dirty
    world.Query<queen::Write<WorldMatrix>, queen::Read<Transform>,
                queen::Read<TransformVersion>, queen::Read<queen::Parent>>()
        .Each([&](WorldMatrix& wm, const Transform& tf,
                  const TransformVersion& ver, const queen::Parent& parent) {
            bool self_dirty = (ver.last_modified == tick || tick <= 1);
            bool parent_dirty = false;
            if (parent.IsValid())
            {
                const auto* pver = world.Get<TransformVersion>(parent.entity);
                if (pver && (pver->last_modified == tick || tick <= 1))
                    parent_dirty = true;
            }

            if (self_dirty || parent_dirty)
            {
                Mat4 local = TRS(tf.position, tf.rotation, tf.scale);
                if (parent.IsValid())
                {
                    const auto* pwm = world.Get<WorldMatrix>(parent.entity);
                    wm.matrix = pwm ? pwm->matrix * local : local;
                }
                else
                {
                    wm.matrix = local;
                }
            }
        });
}

void WorldAABBSystem(queen::World& world)
{
    auto* time = world.Resource<Time>();
    if (!time) return;

    uint32_t tick = static_cast<uint32_t>(time->tick);

    world.Query<queen::Write<WorldAABB>, queen::Read<WorldMatrix>,
                queen::Read<LocalAABB>, queen::Read<TransformVersion>>()
        .Each([&](WorldAABB& waabb, const WorldMatrix& wm,
                  const LocalAABB& local, const TransformVersion& ver) {
            if (ver.last_modified == tick || tick <= 1)
                waabb.bounds = TransformAABB(wm.matrix, local.bounds);
        });
}

} // namespace waggle
