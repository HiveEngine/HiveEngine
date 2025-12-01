#pragma once

namespace wax::detail
{
    template<typename T>
    [[nodiscard]] constexpr T Likely(T value) noexcept
    {
        #if defined(__GNUC__) || defined(__clang__)
            return __builtin_expect(static_cast<bool>(value), 1) ? value : T{};
        #else
            return value;
        #endif
    }

    template<typename T>
    [[nodiscard]] constexpr T Unlikely(T value) noexcept
    {
        #if defined(__GNUC__) || defined(__clang__)
            return __builtin_expect(static_cast<bool>(value), 0) ? value : T{};
        #else
            return value;
        #endif
    }

    template<typename T>
    [[nodiscard]] constexpr T* AssumeAligned(T* ptr, size_t alignment) noexcept
    {
        #if defined(__GNUC__) || defined(__clang__)
            return static_cast<T*>(__builtin_assume_aligned(ptr, alignment));
        #elif defined(_MSC_VER)
            // MSVC doesn't have __builtin_assume_aligned, but alignment is handled differently
            return ptr;
        #else
            return ptr;
        #endif
    }

    template<typename T>
    inline void DoNotOptimizeAway(T const& value)
    {
        #if defined(__GNUC__) || defined(__clang__)
            asm volatile("" : : "r,m"(value) : "memory");
        #elif defined(_MSC_VER)
            _ReadWriteBarrier();
            const volatile void* volatile p = &value;
            (void)p;
        #else
            volatile T copy = value;
            (void)copy;
        #endif
    }
}
