#include <hive/core/assert.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#if HIVE_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

namespace hive
{
    // Thread-local to avoid allocations
    thread_local char s_AssertMessageBuffer[2048];

    bool HandleAssertionFailure(
        const char* file,
        std::uint_least32_t line,
        const char* function,
        const char* message)
    {
        const char* filename = file;
        const char* lastSlash = std::strrchr(file, '/');
        const char* lastBackslash = std::strrchr(file, '\\');

        if (lastSlash && lastBackslash)
        {
            filename = (lastSlash > lastBackslash) ? lastSlash + 1 : lastBackslash + 1;
        }
        else if (lastSlash)
        {
            filename = lastSlash + 1;
        }
        else if (lastBackslash)
        {
            filename = lastBackslash + 1;
        }

        if (message && message[0] != '\0')
        {
            std::snprintf(s_AssertMessageBuffer, sizeof(s_AssertMessageBuffer),
                "Assertion failed\n"
                "  File: %s:%d\n"
                "  Function: %s\n"
                "  Message: %s",
                filename, line, function, message);
        }
        else
        {
            std::snprintf(s_AssertMessageBuffer, sizeof(s_AssertMessageBuffer),
                "Assertion failed\n"
                "  File: %s:%d\n"
                "  Function: %s",
                filename, line, function);
        }

        #if HIVE_PLATFORM_WINDOWS
            OutputDebugStringA(s_AssertMessageBuffer);
            OutputDebugStringA("\n");
        #endif

        std::fprintf(stderr, "%s\n", s_AssertMessageBuffer);

        HIVE_DEBUG_BREAK();

        return false;
    }
}
