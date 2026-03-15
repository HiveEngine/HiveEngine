#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace larvae
{
    using CapabilityMask = uint32_t;

    enum class Capability : CapabilityMask
    {
        NONE = 0,
        HEADLESS = 1u << 0,
        WINDOW = 1u << 1,
        RENDER = 1u << 2
    };

    [[nodiscard]] constexpr CapabilityMask ToMask(Capability capability) noexcept
    {
        return static_cast<CapabilityMask>(capability);
    }

    [[nodiscard]] constexpr CapabilityMask operator|(Capability lhs, Capability rhs) noexcept
    {
        return ToMask(lhs) | ToMask(rhs);
    }

    [[nodiscard]] constexpr CapabilityMask operator|(CapabilityMask lhs, Capability rhs) noexcept
    {
        return lhs | ToMask(rhs);
    }

    [[nodiscard]] constexpr CapabilityMask operator|(Capability lhs, CapabilityMask rhs) noexcept
    {
        return ToMask(lhs) | rhs;
    }

    [[nodiscard]] constexpr CapabilityMask operator&(CapabilityMask lhs, Capability rhs) noexcept
    {
        return lhs & ToMask(rhs);
    }

    [[nodiscard]] constexpr bool HasAllCapabilities(CapabilityMask available, CapabilityMask required) noexcept
    {
        return (available & required) == required;
    }

    [[nodiscard]] CapabilityMask DetectBuildCapabilities() noexcept;
    [[nodiscard]] CapabilityMask ParseCapabilityList(std::string_view text, bool* ok = nullptr) noexcept;
    [[nodiscard]] std::string FormatCapabilities(CapabilityMask mask);
} // namespace larvae
