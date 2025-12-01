#pragma once

#include <cstddef>
#include <hive/core/assert.h>

namespace wax
{
    /**
     * Fixed-size array with bounds checking in debug builds
     *
     * Stack-allocated array wrapper that provides:
     * - Bounds checking in debug mode (zero overhead in release)
     * - Iterator support (begin/end for range-for loops)
     * - Size and data access methods
     * - Drop-in replacement for std::array
     *
     * Performance characteristics:
     * - Storage: Stack-allocated (zero heap allocation)
     * - Access: O(1) - direct array indexing
     * - Iterator: O(1) - pointer arithmetic
     * - Size: O(1) - compile-time constant
     * - Bounds check: O(1) in debug, zero overhead in release
     *
     * Limitations:
     * - Fixed size at compile time (cannot grow/shrink)
     * - Stack-allocated only (cannot be too large)
     * - Size must be > 0 (use Span<T> for dynamic-size views)
     *
     * Example:
     * @code
     *   wax::Array<int, 4> arr = {1, 2, 3, 4};
     *
     *   for (int val : arr) {
     *       // ...
     *   }
     *
     *   arr[0] = 10;  // Bounds-checked in debug
     *   int x = arr.At(1);  // Always bounds-checked
     * @endcode
     */
    template<typename T, size_t N>
    struct Array
    {
        static_assert(N > 0, "Array size must be greater than zero");

        using ValueType = T;
        using SizeType = size_t;
        using Iterator = T*;
        using ConstIterator = const T*;

        T data_[N];

        [[nodiscard]] constexpr T& operator[](size_t index) noexcept
        {
            hive::Assert(index < N, "Array index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr const T& operator[](size_t index) const noexcept
        {
            hive::Assert(index < N, "Array index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr T& At(size_t index)
        {
            hive::Check(index < N, "Array index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr const T& At(size_t index) const
        {
            hive::Check(index < N, "Array index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr T& Front() noexcept
        {
            return data_[0];
        }

        [[nodiscard]] constexpr const T& Front() const noexcept
        {
            return data_[0];
        }

        [[nodiscard]] constexpr T& Back() noexcept
        {
            return data_[N - 1];
        }

        [[nodiscard]] constexpr const T& Back() const noexcept
        {
            return data_[N - 1];
        }

        [[nodiscard]] constexpr T* Data() noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr const T* Data() const noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return N;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return false;  // Array is never empty (N > 0)
        }

        [[nodiscard]] constexpr Iterator begin() noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr ConstIterator begin() const noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr Iterator end() noexcept
        {
            return data_ + N;
        }

        [[nodiscard]] constexpr ConstIterator end() const noexcept
        {
            return data_ + N;
        }

        constexpr void Fill(const T& value)
        {
            if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) == 1)
            {
                if (!std::is_constant_evaluated())
                {
                    std::memset(data_, static_cast<unsigned char>(value), N);
                    return;
                }
            }

            for (size_t i = 0; i < N; ++i)
            {
                data_[i] = value;
            }
        }
    };

    template<typename T, typename... U>
    Array(T, U...) -> Array<T, 1 + sizeof...(U)>;
}
