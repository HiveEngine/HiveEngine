#pragma once

#include <cstdint>
#include <string_view>

namespace nectar
{
    using TypeId = uint64_t;

    namespace detail
    {
        constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
        constexpr uint64_t kFnvPrime = 1099511628211ULL;

        constexpr uint64_t Fnv1a(std::string_view str) noexcept
        {
            uint64_t hash = kFnvOffset;
            for (char c : str)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= kFnvPrime;
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
            static_assert(false, "Unsupported compiler");
#endif
        }
    }

    template<typename T>
    consteval TypeId TypeIdOf() noexcept
    {
        return detail::Fnv1a(detail::RawTypeName<T>());
    }
}
