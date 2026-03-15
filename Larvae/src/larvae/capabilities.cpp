#include <larvae/capabilities.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace larvae
{
    namespace
    {
        struct CapabilityName
        {
            std::string_view m_name;
            Capability m_capability;
        };

        constexpr std::array<CapabilityName, 3> kCapabilityNames{{
            {"headless", Capability::HEADLESS},
            {"window", Capability::WINDOW},
            {"render", Capability::RENDER},
        }};

        [[nodiscard]] std::string_view Trim(std::string_view value) noexcept
        {
            size_t start = 0;
            while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
            {
                ++start;
            }

            size_t end = value.size();
            while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
            {
                --end;
            }

            return value.substr(start, end - start);
        }

        [[nodiscard]] bool EqualsInsensitive(std::string_view lhs, std::string_view rhs) noexcept
        {
            if (lhs.size() != rhs.size())
            {
                return false;
            }

            for (size_t i = 0; i < lhs.size(); ++i)
            {
                const auto left = static_cast<unsigned char>(lhs[i]);
                const auto right = static_cast<unsigned char>(rhs[i]);
                if (std::tolower(left) != std::tolower(right))
                {
                    return false;
                }
            }

            return true;
        }
    } // namespace

    CapabilityMask DetectBuildCapabilities() noexcept
    {
        CapabilityMask mask = ToMask(Capability::HEADLESS);

#if defined(HIVE_FEATURE_GLFW) && HIVE_FEATURE_GLFW
        mask |= Capability::WINDOW;
#endif

#if (defined(HIVE_FEATURE_VULKAN) && HIVE_FEATURE_VULKAN) || (defined(HIVE_FEATURE_D3D12) && HIVE_FEATURE_D3D12)
        mask |= Capability::RENDER;
#endif

        return mask;
    }

    CapabilityMask ParseCapabilityList(std::string_view text, bool* ok) noexcept
    {
        if (ok != nullptr)
        {
            *ok = true;
        }

        const std::string_view trimmed = Trim(text);
        if (trimmed.empty() || EqualsInsensitive(trimmed, "none"))
        {
            return 0;
        }

        if (EqualsInsensitive(trimmed, "auto"))
        {
            return DetectBuildCapabilities();
        }

        CapabilityMask mask = 0;
        size_t start = 0;
        while (start < trimmed.size())
        {
            const size_t comma = trimmed.find(',', start);
            const size_t length = comma == std::string_view::npos ? trimmed.size() - start : comma - start;
            const std::string_view token = Trim(trimmed.substr(start, length));

            bool matched = false;
            for (const CapabilityName& capabilityName : kCapabilityNames)
            {
                if (EqualsInsensitive(token, capabilityName.m_name))
                {
                    mask = mask | capabilityName.m_capability;
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                if (ok != nullptr)
                {
                    *ok = false;
                }
                return 0;
            }

            if (comma == std::string_view::npos)
            {
                break;
            }
            start = comma + 1;
        }

        return mask;
    }

    std::string FormatCapabilities(CapabilityMask mask)
    {
        if (mask == 0)
        {
            return "none";
        }

        std::string result;
        for (const CapabilityName& capabilityName : kCapabilityNames)
        {
            if ((mask & capabilityName.m_capability) == 0)
            {
                continue;
            }

            if (!result.empty())
            {
                result += ",";
            }
            result.append(capabilityName.m_name);
        }

        return result;
    }
} // namespace larvae
