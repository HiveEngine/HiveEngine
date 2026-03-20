#pragma once

namespace wax::detail
{
    // Prefer [[likely]]/[[unlikely]] on branches (C++20) over these functions.
    // These are kept for expression contexts where attributes can't be used.
    [[nodiscard]] constexpr bool Likely(bool value) noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_expect(value, 1);
#else
        return value;
#endif
    }

    [[nodiscard]] constexpr bool Unlikely(bool value) noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_expect(value, 0);
#else
        return value;
#endif
    }

    template <typename T> [[nodiscard]] constexpr T* AssumeAligned(T* ptr, size_t alignment) noexcept
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

    template <typename T> inline void DoNotOptimizeAway(T const& value)
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
} // namespace wax::detail
