#pragma once

namespace queen { class World; }
namespace terra { struct WindowContext; }

namespace antennae
{
    // Poll keyboard + mouse state from the window and update ECS resources.
    // Inserts Keyboard and Mouse resources if they don't exist yet.
    // Call once per frame, after PollEvents(), before simulation.
    void UpdateInput(queen::World& world, terra::WindowContext* window);
}
