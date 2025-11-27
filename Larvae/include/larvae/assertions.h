#pragma once

#include <string>
#include <string_view>
#include <sstream>
#include <cmath>
#include <source_location>
#include <utility>
#include <cstdlib>

namespace larvae
{
    // Handler called when an assertion fails
    // Returns true to continue test execution, false to abort
    using AssertionFailureHandler = bool(*)(const char* message);

    // Set custom assertion failure handler (nullptr = use default abort handler)
    void SetAssertionFailureHandler(AssertionFailureHandler handler);

    // Internal: Handle assertion failure
    // May return if custom handler returns true, otherwise aborts
    void HandleAssertionFailure(const std::string& message);

    std::string FormatAssertionMessage(
        const char* file,
        std::uint_least32_t line,
        const char* expression,
        const std::string& expected_str = "",
        const std::string& actual_str = "",
        const std::string& custom_message = "");

    inline void AssertTrue(
        bool condition,
        const std::source_location& loc = std::source_location::current())
    {
        if (!condition)
        {
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "condition failed"));
        }
    }

    inline void AssertFalse(
        bool condition,
        const std::source_location& loc = std::source_location::current())
    {
        if (condition)
        {
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "condition should be false"));
        }
    }

    template<typename T1, typename T2>
    void AssertEqual(
        const T1& val1,
        const T2& val2,
        const std::source_location& loc = std::source_location::current())
    {
        if (!(val1 == val2))
        {
            std::ostringstream actual_ss, expected_ss;
            actual_ss << val1;
            expected_ss << val2;
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "equality check", expected_ss.str(), actual_ss.str()));
        }
    }

    template<typename T1, typename T2>
    void AssertNotEqual(
        const T1& val1,
        const T2& val2,
        const std::source_location& loc = std::source_location::current())
    {
        if (!(val1 != val2))
        {
            std::ostringstream ss;
            ss << val1;
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "inequality check", "values should differ", ss.str()));
        }
    }

    template<typename T1, typename T2>
    void AssertLessThan(
        const T1& val1,
        const T2& val2,
        const std::source_location& loc = std::source_location::current())
    {
        if (!(val1 < val2))
        {
            std::ostringstream actual_ss, expected_ss;
            actual_ss << val1;
            expected_ss << "< " << val2;
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "less than check", expected_ss.str(), actual_ss.str()));
        }
    }

    template<typename T1, typename T2>
    void AssertLessEqual(
        const T1& val1,
        const T2& val2,
        const std::source_location& loc = std::source_location::current())
    {
        if (!(val1 <= val2))
        {
            std::ostringstream actual_ss, expected_ss;
            actual_ss << val1;
            expected_ss << "<= " << val2;
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "less equal check", expected_ss.str(), actual_ss.str()));
        }
    }

    template<typename T1, typename T2>
    void AssertGreaterThan(
        const T1& val1,
        const T2& val2,
        const std::source_location& loc = std::source_location::current())
    {
        if (!(val1 > val2))
        {
            std::ostringstream actual_ss, expected_ss;
            actual_ss << val1;
            expected_ss << "> " << val2;
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "greater than check", expected_ss.str(), actual_ss.str()));
        }
    }

    template<typename T1, typename T2>
    void AssertGreaterEqual(
        const T1& val1,
        const T2& val2,
        const std::source_location& loc = std::source_location::current())
    {
        if (!(val1 >= val2))
        {
            std::ostringstream actual_ss, expected_ss;
            actual_ss << val1;
            expected_ss << ">= " << val2;
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "greater equal check", expected_ss.str(), actual_ss.str()));
        }
    }

    template<typename T>
    void AssertNull(
        T* ptr,
        const std::source_location& loc = std::source_location::current())
    {
        if (ptr != nullptr)
        {
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "null check", "nullptr", "non-null pointer"));
        }
    }

    template<typename T>
    void AssertNotNull(
        T* ptr,
        const std::source_location& loc = std::source_location::current())
    {
        if (ptr == nullptr)
        {
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "not null check", "non-null pointer", "nullptr"));
        }
    }

    template<typename T1, typename T2, typename Epsilon>
    void AssertNear(
        const T1& val1,
        const T2& val2,
        const Epsilon& epsilon,
        const std::source_location& loc = std::source_location::current())
    {
        if (std::abs(val1 - val2) > epsilon)
        {
            std::ostringstream ss;
            ss << "Expected: " << val2 << " ± " << epsilon << "\n";
            ss << "Actual: " << val1 << "\n";
            ss << "Difference: " << std::abs(val1 - val2);
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "near check", ss.str(), ""));
        }
    }

    inline void AssertFloatEqual(
        float val1,
        float val2,
        const std::source_location& loc = std::source_location::current())
    {
        AssertNear(val1, val2, 1e-5f, loc);
    }

    inline void AssertDoubleEqual(
        double val1,
        double val2,
        const std::source_location& loc = std::source_location::current())
    {
        AssertNear(val1, val2, 1e-9, loc);
    }

    inline void AssertStringEqual(
        std::string_view str1,
        std::string_view str2,
        const std::source_location& loc = std::source_location::current())
    {
        if (str1 != str2)
        {
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "string equality",
                                       std::string{str2}, std::string{str1}));
        }
    }

    inline void AssertStringNotEqual(
        std::string_view str1,
        std::string_view str2,
        const std::source_location& loc = std::source_location::current())
    {
        if (str1 == str2)
        {
            HandleAssertionFailure(
                FormatAssertionMessage(loc.file_name(), loc.line(), "string inequality",
                                       "different strings", std::string{str1}));
        }
    }
}
