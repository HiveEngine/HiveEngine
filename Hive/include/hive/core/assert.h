#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #define HIVE_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define HIVE_PLATFORM_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define HIVE_PLATFORM_MACOS 1
#else
    #define HIVE_PLATFORM_UNKNOWN 1
#endif

#if defined(_MSC_VER)
    #define HIVE_COMPILER_MSVC 1
#elif defined(__clang__)
    #define HIVE_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define HIVE_COMPILER_GCC 1
#endif

#if defined(NDEBUG) || defined(_NDEBUG)
    #define HIVE_BUILD_RELEASE 1
    #define HIVE_BUILD_DEBUG 0
#else
    #define HIVE_BUILD_RELEASE 0
    #define HIVE_BUILD_DEBUG 1
#endif

#if HIVE_COMPILER_MSVC
    #define HIVE_DEBUG_BREAK() __debugbreak()
#elif HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #if HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
        #include <signal.h>
        #define HIVE_DEBUG_BREAK() raise(SIGTRAP)
    #else
        #define HIVE_DEBUG_BREAK() __builtin_trap()
    #endif
#else
    #define HIVE_DEBUG_BREAK() ((void)0)
#endif

#if HIVE_COMPILER_MSVC
    #define HIVE_FORCE_INLINE __forceinline
#elif HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #define HIVE_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define HIVE_FORCE_INLINE inline
#endif

#if HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #define HIVE_LIKELY(x) __builtin_expect(!!(x), 1)
    #define HIVE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define HIVE_LIKELY(x) (x)
    #define HIVE_UNLIKELY(x) (x)
#endif

#include <source_location>
#include <type_traits>
#include <cstdint>
#include <cstdlib>

namespace hive
{
    bool HandleAssertionFailure(
        const char* file,
        std::uint_least32_t line,
        const char* function,
        const char* message = nullptr);

    // Assert: Debug only, zero cost in release, expr not evaluated in release
    // NOTE: Currently only checks at runtime. In C++26, we could add compile-time
    // validation using consteval diagnostics for better error messages when used
    // in constexpr contexts.
#if HIVE_BUILD_DEBUG
    constexpr void Assert(
        bool expr,
        const char* message = nullptr,
        const std::source_location& loc = std::source_location::current())
    {
        if (!std::is_constant_evaluated())
        {
            if (HIVE_UNLIKELY(!expr))
            {
                HandleAssertionFailure(loc.file_name(), loc.line(), loc.function_name(), message);
            }
        }
    }
#else
    constexpr void Assert(bool, const char* = nullptr, const std::source_location& = std::source_location::current()) {}
#endif

    // Verify: Always evaluates expr (even in release), reports failure in debug only
#if HIVE_BUILD_DEBUG
    constexpr bool Verify(
        bool expr,
        const char* message = nullptr,
        const std::source_location& loc = std::source_location::current())
    {
        if (!std::is_constant_evaluated())
        {
            if (HIVE_UNLIKELY(!expr))
            {
                HandleAssertionFailure(loc.file_name(), loc.line(), loc.function_name(), message);
            }
        }
        return expr;
    }
#else
    constexpr bool Verify(bool expr, const char* = nullptr, const std::source_location& = std::source_location::current())
    {
        return expr;
    }
#endif

    // Check: Always evaluates and reports, even in release (use sparingly)
    constexpr void Check(
        bool expr,
        const char* message = nullptr,
        const std::source_location& loc = std::source_location::current())
    {
        if (!std::is_constant_evaluated())
        {
            if (HIVE_UNLIKELY(!expr))
            {
                HandleAssertionFailure(loc.file_name(), loc.line(), loc.function_name(), message);
            }
        }
    }

    // Unreachable: Marks code paths that should never execute
#if HIVE_BUILD_DEBUG
    [[noreturn]] inline void Unreachable(
        const std::source_location& loc = std::source_location::current())
    {
        HandleAssertionFailure(loc.file_name(), loc.line(), loc.function_name(), "Unreachable code executed");
        HIVE_DEBUG_BREAK();
        std::abort();
    }
#else
    [[noreturn]] inline void Unreachable(const std::source_location& = std::source_location::current())
    {
    #if HIVE_COMPILER_MSVC
        __assume(0);
    #elif HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
        __builtin_unreachable();
    #else
        std::abort();
    #endif
    }
#endif

    // NotImplemented: Marks functionality that hasn't been implemented yet
    [[noreturn]] inline void NotImplemented(
        const std::source_location& loc = std::source_location::current())
    {
        HandleAssertionFailure(loc.file_name(), loc.line(), loc.function_name(), "Not implemented");
        HIVE_DEBUG_BREAK();
        std::abort();
    }
}


