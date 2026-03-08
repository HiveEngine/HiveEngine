#pragma once

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/core/entity.h>

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
    template <comb::Allocator Allocator> class ChildrenT
    {
    public:
        explicit ChildrenT(Allocator& alloc)
            : m_entities{alloc} {}

        ChildrenT(const ChildrenT&) = delete;
        ChildrenT& operator=(const ChildrenT&) = delete;

        ChildrenT(ChildrenT&& other) noexcept = default;
        ChildrenT& operator=(ChildrenT&& other) noexcept = default;

        /**
         * Add a child entity to the list
         *
         * @param child Entity to add
         */
        void Add(Entity child) { m_entities.PushBack(child); }

        /**
         * Remove a child entity from the list
         *
         * Uses swap-remove for O(1) removal (order not preserved).
         *
         * @param child Entity to remove
         * @return true if child was found and removed
         */
        bool Remove(Entity child) {
            for (size_t i = 0; i < m_entities.Size(); ++i)
            {
                if (m_entities[i] == child)
                {
                    // Swap with last and pop (O(1) removal)
                    if (i < m_entities.Size() - 1)
                    {
                        m_entities[i] = m_entities.Back();
                    }
                    m_entities.PopBack();
                    return true;
                }
            }
            return false;
        }

        /**
         * Check if a child entity is in the list
         */
        [[nodiscard]] bool Contains(Entity child) const {
            for (size_t i = 0; i < m_entities.Size(); ++i)
            {
                if (m_entities[i] == child)
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * Get child at index
         */
        [[nodiscard]] Entity At(size_t index) const { return m_entities[index]; }

        /**
         * Get number of children
         */
        [[nodiscard]] size_t Count() const noexcept { return m_entities.Size(); }

        /**
         * Check if there are no children
         */
        [[nodiscard]] bool IsEmpty() const noexcept { return m_entities.IsEmpty(); }

        // ─────────────────────────────────────────────────────────────
        // Iterator support
        // ─────────────────────────────────────────────────────────────

        [[nodiscard]] const Entity* Begin() const noexcept { return m_entities.Begin(); }

        [[nodiscard]] const Entity* End() const noexcept { return m_entities.End(); }

        [[nodiscard]] Entity* Begin() noexcept { return m_entities.Begin(); }

        [[nodiscard]] Entity* End() noexcept { return m_entities.End(); }

    private:
        wax::Vector<Entity> m_entities;
    };
} // namespace queen
