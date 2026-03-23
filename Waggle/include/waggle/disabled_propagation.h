#pragma once

#include <hive/hive_config.h>

#include <queen/core/entity.h>

namespace queen
{
    class World;
}

namespace waggle
{
    HIVE_API void RegisterDisabledObservers(queen::World& world);
    HIVE_API void SetEntityDisabled(queen::World& world, queen::Entity entity, bool disabled);
    HIVE_API void RecalculateEntityHierarchyDisabled(queen::World& world, queen::Entity entity);
} // namespace waggle
