#pragma once

#include <cstdint>

namespace queen
{
    /**
     * Flags for field display and behavior in the editor inspector
     *
     * Combinable via bitwise OR. Used in FieldAttributes::flags.
     */
    enum class FieldFlag : uint32_t
    {
        NONE = 0,
        HIDDEN = 1 << 0,    // Not shown in inspector
        READ_ONLY = 1 << 1, // Shown but not editable
        COLOR = 1 << 2,     // Display as color picker (Float3/Float4)
        ANGLE = 1 << 3,     // Display in degrees (stored as radians)
        FILE_PATH = 1 << 4, // Display file browser dialog
        NO_DELTA = 1 << 5,  // Excluded from network delta compression
    };

    [[nodiscard]] constexpr FieldFlag operator|(FieldFlag a, FieldFlag b) noexcept {
        return static_cast<FieldFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    [[nodiscard]] constexpr FieldFlag operator&(FieldFlag a, FieldFlag b) noexcept {
        return static_cast<FieldFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    /**
     * Editor annotations for a reflected field
     *
     * Optional metadata attached to fields via the FieldBuilder chaining API.
     * When nullptr (default), the field uses default editor display.
     *
     * Memory layout: 48 bytes
     */
    struct FieldAttributes
    {
        float m_min = 0.f;
        float m_max = 0.f;
        float m_step = 0.f;
        const char* m_tooltip = nullptr;
        const char* m_category = nullptr;
        const char* m_displayName = nullptr;
        uint32_t m_flags = 0;

        [[nodiscard]] constexpr bool HasRange() const noexcept { return m_min != m_max; }

        [[nodiscard]] constexpr bool HasFlag(FieldFlag flag) const noexcept {
            return (m_flags & static_cast<uint32_t>(flag)) != 0;
        }
    };
} // namespace queen
