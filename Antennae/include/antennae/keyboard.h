#pragma once

#include <terra/input/keys.h>

namespace antennae
{
    struct Keyboard
    {
        static constexpr int kKeyCount = static_cast<int>(terra::Key::COUNT);

        bool m_current[kKeyCount]{};
        bool m_previous[kKeyCount]{};

        [[nodiscard]] bool IsDown(terra::Key k) const noexcept
        {
            return m_current[static_cast<int>(k)];
        }

        [[nodiscard]] bool JustPressed(terra::Key k) const noexcept
        {
            int i = static_cast<int>(k);
            return m_current[i] && !m_previous[i];
        }

        [[nodiscard]] bool JustReleased(terra::Key k) const noexcept
        {
            int i = static_cast<int>(k);
            return !m_current[i] && m_previous[i];
        }
    };
} // namespace antennae
