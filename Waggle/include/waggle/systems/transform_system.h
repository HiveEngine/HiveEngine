#pragma once

#include <hive/hive_config.h>

namespace queen
{
    class World;
}

namespace waggle
{

    HIVE_API void TransformSystem(queen::World& world);
    HIVE_API void WorldAABBSystem(queen::World& world);
    HIVE_API void RegisterTransformObservers(queen::World& world);

    // Must be called at startup and after ClearSystems() (hot-reload).
    HIVE_API void RegisterEngineSystems(queen::World& world);

} // namespace waggle
