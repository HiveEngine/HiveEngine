#pragma once

#include <terra/input/keys.h>

namespace antennae
{
    struct Mouse
    {
        static constexpr int kButtonCount = static_cast<int>(terra::MouseButton::COUNT);

        float m_x{}, m_y{};
        float m_dx{}, m_dy{};
        float m_scrollX{}, m_scrollY{};

        bool m_buttons[kButtonCount]{};
        bool m_prevButtons[kButtonCount]{};
        bool m_firstUpdate{true};

        [[nodiscard]] bool IsDown(terra::MouseButton b) const noexcept
        {
            return m_buttons[static_cast<int>(b)];
        }

        [[nodiscard]] bool JustPressed(terra::MouseButton b) const noexcept
        {
            int i = static_cast<int>(b);
            return m_buttons[i] && !m_prevButtons[i];
        }

        [[nodiscard]] bool JustReleased(terra::MouseButton b) const noexcept
        {
            int i = static_cast<int>(b);
            return !m_buttons[i] && m_prevButtons[i];
        }
    };
} // namespace antennae
