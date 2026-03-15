#pragma once

#include <larvae/capabilities.h>

#include <cstdint>
#include <functional>
#include <string>

namespace larvae
{
    struct TestInfo
    {
        std::string suite_name;
        std::string test_name;
        std::function<void()> func;
        const char* file;
        std::uint_least32_t line;
        CapabilityMask required_capabilities{0};

        [[nodiscard]] std::string GetFullName() const
        {
            return suite_name + "." + test_name;
        }
    };
} // namespace larvae
