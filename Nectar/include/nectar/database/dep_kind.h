#pragma once

#include <cstdint>

namespace nectar
{
    /// Types of asset dependencies.
    /// Used as bitmask flags for filtering.
    enum class DepKind : uint8_t
    {
        Hard  = 1,  // A needs B to exist (load B before A)
        Soft  = 2,  // A can use B but works without
        Build = 4,  // A depends on B at cook time only
        All   = 7
    };

    [[nodiscard]] constexpr DepKind operator|(DepKind a, DepKind b) noexcept
    {
        return static_cast<DepKind>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    [[nodiscard]] constexpr DepKind operator&(DepKind a, DepKind b) noexcept
    {
        return static_cast<DepKind>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    [[nodiscard]] constexpr bool HasFlag(DepKind mask, DepKind flag) noexcept
    {
        return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(flag)) != 0;
    }
}
