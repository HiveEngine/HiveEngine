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

} // namespace waggle
