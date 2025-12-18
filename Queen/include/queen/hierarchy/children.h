#pragma once

#include <queen/core/entity.h>
#include <wax/containers/vector.h>
#include <comb/allocator_concepts.h>

namespace queen
{
    /**
     * Cached list of child entities
     *
     * The Children component maintains a list of all entities that have
     * this entity as their Parent. This is automatically managed by the
     * hierarchy system - users should not modify it directly.
     *
     * Use cases:
     * - Fast child iteration (O(n) where n = children count)
     * - Cascading operations (destroy all children)
     * - Transform propagation (update all children)
     *
     * Memory layout:
     * +----------------------------------------------------------------+
     * | entities: Vector<Entity> (~24 bytes + heap data)               |
     * +----------------------------------------------------------------+
     *
     * Performance characteristics:
     * - Get children: O(1) - direct component access
     * - Iterate children: O(n) where n = child count
     * - Add child: O(1) amortized (vector push)
     * - Remove child: O(n) (linear search + swap-remove)
     *
     * Limitations:
     * - Managed automatically - do not modify directly
     * - Order not guaranteed (swap-remove on child removal)
     *
     * Example:
     * @code
     *   // Iterate children
     *   if (const Children* children = world.GetChildren(parent)) {
     *       for (size_t i = 0; i < children->Count(); ++i) {
     *           Entity child = children->At(i);
     *           // Process child
     *       }
     *   }
     *
     *   // Or use helper
     *   world.ForEachChild(parent, [](Entity child) {
     *       // Process child
     *   });
     * @endcode
     */
    template<comb::Allocator Allocator>
    class ChildrenT
    {
    public:
        explicit ChildrenT(Allocator& alloc)
            : entities_{alloc}
        {
        }

        ChildrenT(const ChildrenT&) = delete;
        ChildrenT& operator=(const ChildrenT&) = delete;

        ChildrenT(ChildrenT&& other) noexcept = default;
        ChildrenT& operator=(ChildrenT&& other) noexcept = default;

        /**
         * Add a child entity to the list
         *
         * @param child Entity to add
         */
        void Add(Entity child)
        {
            entities_.PushBack(child);
        }

        /**
         * Remove a child entity from the list
         *
         * Uses swap-remove for O(1) removal (order not preserved).
         *
         * @param child Entity to remove
         * @return true if child was found and removed
         */
        bool Remove(Entity child)
        {
            for (size_t i = 0; i < entities_.Size(); ++i)
            {
                if (entities_[i] == child)
                {
                    // Swap with last and pop (O(1) removal)
                    if (i < entities_.Size() - 1)
                    {
                        entities_[i] = entities_.Back();
                    }
                    entities_.PopBack();
                    return true;
                }
            }
            return false;
        }

        /**
         * Check if a child entity is in the list
         */
        [[nodiscard]] bool Contains(Entity child) const
        {
            for (size_t i = 0; i < entities_.Size(); ++i)
            {
                if (entities_[i] == child)
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * Get child at index
         */
        [[nodiscard]] Entity At(size_t index) const
        {
            return entities_[index];
        }

        /**
         * Get number of children
         */
        [[nodiscard]] size_t Count() const noexcept
        {
            return entities_.Size();
        }

        /**
         * Check if there are no children
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return entities_.IsEmpty();
        }

        // ─────────────────────────────────────────────────────────────
        // Iterator support
        // ─────────────────────────────────────────────────────────────

        [[nodiscard]] const Entity* begin() const noexcept
        {
            return entities_.begin();
        }

        [[nodiscard]] const Entity* end() const noexcept
        {
            return entities_.end();
        }

        [[nodiscard]] Entity* begin() noexcept
        {
            return entities_.begin();
        }

        [[nodiscard]] Entity* end() noexcept
        {
            return entities_.end();
        }

    private:
        wax::Vector<Entity, Allocator> entities_;
    };
}
