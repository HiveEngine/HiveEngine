#include <larvae/precomp.h>
#include <larvae/assertions.h>
#include <sstream>
#include <cstdlib>
#include <cstdio>

namespace larvae
{
    namespace
    {
        // Global assertion failure handler
        AssertionFailureHandler g_assertion_handler = nullptr;
    }

    void SetAssertionFailureHandler(AssertionFailureHandler handler)
    {
        g_assertion_handler = handler;
    }

    void HandleAssertionFailure(const std::string& message)
    {
        if (g_assertion_handler)
        {
            // Custom handler decides whether to continue or abort
            // If it returns true, we continue; if false, we abort
            if (g_assertion_handler(message.c_str()))
            {
                return; // Handler handled it, continue execution
            }
        }

        // Default behavior: print and abort
        std::fprintf(stderr, "%s\n", message.c_str());
        std::fflush(stderr);
        std::abort();
    }

    std::string FormatAssertionMessage(
        const char* file,
        std::uint_least32_t line,
        const char* expression,
        const std::string& expected_str,
        const std::string& actual_str,
        const std::string& custom_message)
    {
        std::ostringstream ss;
        ss << file << ":" << line << ": Assertion failed\n";
        ss << "  Expression: " << expression << "\n";

        if (!expected_str.empty())
        {
            ss << "  Expected: " << expected_str << "\n";
        }

        if (!actual_str.empty())
        {
            ss << "  Actual: " << actual_str << "\n";
        }

        if (!custom_message.empty())
        {
            ss << "  Message: " << custom_message << "\n";
        }

        return ss.str();
    }
}
