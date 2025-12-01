#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace queen
{
    /**
     * Compile-time type identifier
     *
     * Generates unique 64-bit identifiers for types at compile-time using
     * FNV-1a hash of the type name. Zero runtime overhead for type lookups.
     *
     * Performance characteristics:
     * - TypeId generation: Compile-time (zero runtime cost)
     * - TypeId comparison: O(1) - single integer compare
     * - Hash collisions: Extremely rare with 64-bit FNV-1a
     *
     * Limitations:
     * - Relies on compiler-specific __PRETTY_FUNCTION__ or __FUNCSIG__
     * - Different compilers may produce different hashes for same type
     * - Not stable across compiler versions (recompilation required)
     *
     * Example:
     * @code
     *   constexpr TypeId posId = queen::TypeIdOf<Position>();
     *   constexpr TypeId velId = queen::TypeIdOf<Velocity>();
     *   static_assert(posId != velId);
     *
     *   if (TypeIdOf<T>() == posId) { ... }
     * @endcode
     */
    using TypeId = uint64_t;

    constexpr TypeId kInvalidTypeId = 0;

    namespace detail
    {
        constexpr uint64_t kFnv1aOffset = 14695981039346656037ULL;
        constexpr uint64_t kFnv1aPrime = 1099511628211ULL;

        constexpr uint64_t Fnv1aHash(std::string_view str) noexcept
        {
            uint64_t hash = kFnv1aOffset;
            for (char c : str)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= kFnv1aPrime;
            }
            return hash;
        }

        template<typename T>
        constexpr std::string_view RawTypeName() noexcept
        {
#if defined(__clang__) || defined(__GNUC__)
            return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
            return __FUNCSIG__;
#else
            static_assert(false, "Unsupported compiler for TypeId");
#endif
        }

        template<typename T>
        constexpr std::string_view TypeName() noexcept
        {
            constexpr std::string_view raw = RawTypeName<T>();

#if defined(__clang__)
            constexpr std::string_view prefix = "std::string_view queen::detail::RawTypeName() [T = ";
            constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
            constexpr std::string_view prefix = "constexpr std::string_view queen::detail::RawTypeName() [with T = ";
            constexpr std::string_view suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
            constexpr std::string_view prefix = "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl queen::detail::RawTypeName<";
            constexpr std::string_view suffix = ">(void) noexcept";
#endif

            std::string_view name = raw;
            if (name.starts_with(prefix))
            {
                name.remove_prefix(prefix.size());
            }
            if (name.ends_with(suffix))
            {
                name.remove_suffix(suffix.size());
            }
            return name;
        }
    }

    /**
     * Get compile-time type ID for type T
     */
    template<typename T>
    consteval TypeId TypeIdOf() noexcept
    {
        return detail::Fnv1aHash(detail::RawTypeName<T>());
    }

    /**
     * Get human-readable type name for debugging
     */
    template<typename T>
    constexpr std::string_view TypeNameOf() noexcept
    {
        return detail::TypeName<T>();
    }
}
