#pragma once

#include <queen/core/entity.h>

namespace queen
{
    // Single-parent reference, managed by the hierarchy system.
    // Use World::SetParent / World::RemoveParent, not direct mutation.
    struct Parent
    {
        Entity m_entity;

        Parent() noexcept
            : m_entity{Entity::Invalid()}
        {
        }
        explicit Parent(Entity e) noexcept
            : m_entity{e}
        {
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return !m_entity.IsNull();
        }

        [[nodiscard]] bool operator==(const Parent& other) const noexcept
        {
            return m_entity == other.m_entity;
        }
    };
} // namespace queen
