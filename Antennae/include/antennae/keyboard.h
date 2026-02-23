#pragma once

#include <terra/input/keys.h>

namespace antennae
{
    struct Keyboard
    {
        static constexpr int kKeyCount = static_cast<int>(terra::Key::COUNT);

        bool current[kKeyCount]{};
        bool previous[kKeyCount]{};

        [[nodiscard]] bool IsDown(terra::Key k) const
        {
            return current[static_cast<int>(k)];
        }

        [[nodiscard]] bool JustPressed(terra::Key k) const
        {
            int i = static_cast<int>(k);
            return current[i] && !previous[i];
        }

        [[nodiscard]] bool JustReleased(terra::Key k) const
        {
            int i = static_cast<int>(k);
            return !current[i] && previous[i];
        }
    };
}
