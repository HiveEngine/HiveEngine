#pragma once

#include <terra/input/keys.h>

namespace antennae
{
    struct Mouse
    {
        static constexpr int kButtonCount = static_cast<int>(terra::MouseButton::COUNT);

        float x{}, y{};
        float dx{}, dy{};
        float scroll_x{}, scroll_y{};

        bool buttons[kButtonCount]{};
        bool prev_buttons[kButtonCount]{};
        bool first_update{true};

        [[nodiscard]] bool IsDown(terra::MouseButton b) const
        {
            return buttons[static_cast<int>(b)];
        }

        [[nodiscard]] bool JustPressed(terra::MouseButton b) const
        {
            int i = static_cast<int>(b);
            return buttons[i] && !prev_buttons[i];
        }

        [[nodiscard]] bool JustReleased(terra::MouseButton b) const
        {
            int i = static_cast<int>(b);
            return !buttons[i] && prev_buttons[i];
        }
    };
}
