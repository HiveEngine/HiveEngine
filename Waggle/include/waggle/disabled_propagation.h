#pragma once

#include <queen/core/entity.h>

namespace queen
{
    class World;
}

namespace waggle
{
    void RegisterDisabledObservers(queen::World& world);
    void SetEntityDisabled(queen::World& world, queen::Entity entity, bool disabled);
    void RecalculateEntityHierarchyDisabled(queen::World& world, queen::Entity entity);
} // namespace waggle
