#pragma once

#include <hive/hive_config.h>

#include <antennae/actions.h>
#include <antennae/keyboard.h>
#include <antennae/mouse.h>

namespace queen
{
    class World;
}
namespace terra
{
    struct InputState;
    struct WindowContext;
} // namespace terra

namespace antennae
{
    HIVE_API void UpdateKeyboard(Keyboard& keyboard, const terra::InputState* input);

    HIVE_API void UpdateMouse(Mouse& mouse, const terra::InputState* input);

    // Poll keyboard + mouse state from the window and update ECS resources.
    // Inserts Keyboard, Mouse, InputActionMap and InputActions resources if they don't exist yet.
    // Call once per frame, after PollEvents(), before simulation.
    HIVE_API void UpdateInput(queen::World& world, terra::WindowContext* window);
} // namespace antennae
