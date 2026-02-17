#pragma once

#include <queen/core/entity.h>

namespace queen
{
    /**
     * Marks an entity as having a parent
     *
     * The Parent component stores a reference to the parent entity.
     * When an entity has a Parent component, it is considered a child
     * of the referenced entity.
     *
     * Use cases:
     * - Scene graph hierarchies (transforms)
     * - UI widget trees
     * - Equipment attachment (sword -> character hand)
     * - Particle system ownership
     *
     * Memory layout:
     * +----------------------------------------+
     * | entity: Entity (8 bytes)               |
     * +----------------------------------------+
     *
     * Performance characteristics:
     * - Get parent: O(1) - direct component access
     * - Set parent: O(n) - archetype transition + children update
     * - Memory: 8 bytes per entity with parent
     *
     * Limitations:
     * - Single parent only (no multi-parent graphs)
     * - Circular hierarchies not prevented (user responsibility)
     * - Parent must exist when setting (validated in debug)
     *
     * Example:
     * @code
     *   // Set parent
     *   world.SetParent(child, parent);
     *
     *   // Query entities with parents
     *   world.Query<Read<Parent>, Read<Position>>()
     *       .Each([](const Parent& p, const Position& pos) {
     *           // Child with position
     *       });
     *
     *   // Get parent directly
     *   Entity parent = world.GetParent(child);
     * @endcode
     */
    struct Parent
    {
        Entity entity;

        Parent() noexcept : entity{Entity::Invalid()} {}
        explicit Parent(Entity e) noexcept : entity{e} {}

        [[nodiscard]] bool IsValid() const noexcept { return !entity.IsNull(); }

        [[nodiscard]] bool operator==(const Parent& other) const noexcept
        {
            return entity == other.entity;
        }
    };
}
