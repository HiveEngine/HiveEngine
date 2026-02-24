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
        None     = 0,
        Hidden   = 1 << 0,   // Not shown in inspector
        ReadOnly = 1 << 1,   // Shown but not editable
        Color    = 1 << 2,   // Display as color picker (Float3/Float4)
        Angle    = 1 << 3,   // Display in degrees (stored as radians)
        FilePath = 1 << 4,   // Display file browser dialog
        NoDelta  = 1 << 5,   // Excluded from network delta compression
    };

    [[nodiscard]] constexpr FieldFlag operator|(FieldFlag a, FieldFlag b) noexcept
    {
        return static_cast<FieldFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    [[nodiscard]] constexpr FieldFlag operator&(FieldFlag a, FieldFlag b) noexcept
    {
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
        float min = 0.f;
        float max = 0.f;
        float step = 0.f;
        const char* tooltip = nullptr;
        const char* category = nullptr;
        const char* display_name = nullptr;
        uint32_t flags = 0;

        [[nodiscard]] constexpr bool HasRange() const noexcept
        {
            return min != max;
        }

        [[nodiscard]] constexpr bool HasFlag(FieldFlag flag) const noexcept
        {
            return (flags & static_cast<uint32_t>(flag)) != 0;
        }
    };
}
