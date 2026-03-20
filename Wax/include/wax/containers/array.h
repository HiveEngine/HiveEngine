#pragma once

#include <hive/core/assert.h>

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace wax
{
    // Fixed-size array with bounds checking in debug builds.
    // Drop-in replacement for std::array with Assert/Check semantics.
    template <typename T, size_t N> struct Array
    {
        static_assert(N > 0, "Array size must be greater than zero");

        using ValueType = T;
        using SizeType = size_t;
        using Iterator = T*;
        using ConstIterator = const T*;

        T m_data[N];

        [[nodiscard]] constexpr T& operator[](size_t index) noexcept
        {
            hive::Assert(index < N, "Array index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr const T& operator[](size_t index) const noexcept
        {
            hive::Assert(index < N, "Array index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr T& At(size_t index)
        {
            hive::Check(index < N, "Array index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr const T& At(size_t index) const
        {
            hive::Check(index < N, "Array index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr T& Front() noexcept
        {
            return m_data[0];
        }

        [[nodiscard]] constexpr const T& Front() const noexcept
        {
            return m_data[0];
        }

        [[nodiscard]] constexpr T& Back() noexcept
        {
            return m_data[N - 1];
        }

        [[nodiscard]] constexpr const T& Back() const noexcept
        {
            return m_data[N - 1];
        }

        [[nodiscard]] constexpr T* Data() noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr const T* Data() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return N;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return false;
        }

        [[nodiscard]] constexpr Iterator Begin() noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr ConstIterator Begin() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr Iterator End() noexcept
        {
            return m_data + N;
        }

        [[nodiscard]] constexpr ConstIterator End() const noexcept
        {
            return m_data + N;
        }

        [[nodiscard]] constexpr Iterator begin() noexcept
        {
            return Begin();
        }

        [[nodiscard]] constexpr ConstIterator begin() const noexcept
        {
            return Begin();
        }

        [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
        {
            return Begin();
        }

        [[nodiscard]] constexpr Iterator end() noexcept
        {
            return End();
        }

        [[nodiscard]] constexpr ConstIterator end() const noexcept
        {
            return End();
        }

        [[nodiscard]] constexpr ConstIterator cend() const noexcept
        {
            return End();
        }

        constexpr void Fill(const T& value)
        {
            if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) == 1)
            {
                if (!std::is_constant_evaluated())
                {
                    std::memset(m_data, static_cast<unsigned char>(value), N);
                    return;
                }
            }

            for (size_t i = 0; i < N; ++i)
            {
                m_data[i] = value;
            }
        }
    };

    template <typename T, typename... U> Array(T, U...) -> Array<T, 1 + sizeof...(U)>;
} // namespace wax
