#pragma once

#include <hive/HiveConfig.h>

// ============================================================================
// Compiler Intrinsics
// ============================================================================

#if HIVE_COMPILER_MSVC
    #define HIVE_INTERNAL_DEBUG_BREAK() __debugbreak()
#elif HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #if HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
        #include <signal.h>
        #define HIVE_INTERNAL_DEBUG_BREAK() raise(SIGTRAP)
    #else
        #define HIVE_INTERNAL_DEBUG_BREAK() __builtin_trap()
    #endif
#else
    #define HIVE_INTERNAL_DEBUG_BREAK() ((void)0)
#endif

#if HIVE_COMPILER_MSVC
    #define HIVE_INTERNAL_FORCE_INLINE __forceinline
#elif HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #define HIVE_INTERNAL_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define HIVE_INTERNAL_FORCE_INLINE inline
#endif

#if HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #define HIVE_INTERNAL_LIKELY(x) __builtin_expect(!!(x), 1)
    #define HIVE_INTERNAL_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define HIVE_INTERNAL_LIKELY(x) (x)
    #define HIVE_INTERNAL_UNLIKELY(x) (x)
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
#if HIVE_FEATURE_ASSERTS
    constexpr void Assert(
        bool expr,
        const char* message = nullptr,
        const std::source_location& loc = std::source_location::current())
    {
        if (!std::is_constant_evaluated())
        {
            if (HIVE_INTERNAL_UNLIKELY(!expr))
            {
                HandleAssertionFailure(loc.file_name(), loc.line(), loc.function_name(), message);
            }
        }
    }
#else
    constexpr void Assert(bool, const char* = nullptr, const std::source_location& = std::source_location::current()) {}
#endif

    // Verify: Always evaluates expr (even in release), reports failure in debug only
#if HIVE_FEATURE_ASSERTS
    constexpr bool Verify(
        bool expr,
        const char* message = nullptr,
        const std::source_location& loc = std::source_location::current())
    {
        if (!std::is_constant_evaluated())
        {
            if (HIVE_INTERNAL_UNLIKELY(!expr))
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
            if (HIVE_INTERNAL_UNLIKELY(!expr))
            {
                HandleAssertionFailure(loc.file_name(), loc.line(), loc.function_name(), message);
            }
        }
    }

    // Unreachable: Marks code paths that should never execute
#if HIVE_FEATURE_ASSERTS
    [[noreturn]] inline void Unreachable(
        const std::source_location& loc = std::source_location::current())
    {
        HandleAssertionFailure(loc.file_name(), loc.line(), loc.function_name(), "Unreachable code executed");
        HIVE_INTERNAL_DEBUG_BREAK();
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
        HIVE_INTERNAL_DEBUG_BREAK();
        std::abort();
    }
}


