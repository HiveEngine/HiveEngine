#pragma once

#include <larvae/capabilities.h>

#include <cstdint>
#include <functional>
#include <string>

namespace larvae
{
    struct TestInfo
    {
        std::string m_suiteName;
        std::string m_testName;
        std::function<void()> m_func;
        const char* m_file;
        std::uint_least32_t m_line;
        CapabilityMask m_requiredCapabilities{0};

        [[nodiscard]] std::string GetFullName() const
        {
            return m_suiteName + "." + m_testName;
        }
    };
} // namespace larvae
