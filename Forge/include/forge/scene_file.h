#pragma once

#include <queen/reflect/component_registry.h>

namespace queen { class World; }

namespace forge
{
    // Save the world to a .hscene JSON file.
    // Returns true on success.
    bool SaveScene(queen::World& world, const queen::ComponentRegistry<256>& registry,
                   const char* path);

    // Load a .hscene JSON file into the world.
    // The world is NOT cleared first (additive load). Clear it yourself if needed.
    // Returns true on success.
    bool LoadScene(queen::World& world, const queen::ComponentRegistry<256>& registry,
                   const char* path);
}
