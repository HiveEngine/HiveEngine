#pragma once

#include <functional>
#include <string>
#include <cstdint>

namespace larvae
{
    struct TestInfo
    {
        std::string suite_name;
        std::string test_name;
        std::function<void()> func;
        const char* file;
        std::uint_least32_t line;

        [[nodiscard]] std::string GetFullName() const
        {
            return suite_name + "." + test_name;
        }
    };
}
