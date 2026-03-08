#pragma once

#include <hive/core/assert.h>

#include <wax/containers/array.h>

#include <cstddef>

namespace wax
{
    /**
     * Non-owning view over a contiguous sequence of elements
     *
     * Span provides a lightweight reference to an existing array without
     * owning the data. It's essentially a pointer + size with bounds checking.
     *
     * Performance characteristics:
     * - Storage: 16 bytes (pointer + size) on 64-bit systems
     * - Access: O(1) - direct pointer arithmetic
     * - Construction: O(1) - just stores pointer and size
     * - Copy: O(1) - trivially copyable
     * - Bounds check: O(1) in debug, zero overhead in release
     *
     * Limitations:
     * - Non-owning (caller must ensure data stays alive)
     * - No memory management (just a view)
     * - Cannot grow/shrink (fixed at construction)
     * - Dangling pointer risk (if source data is destroyed)
     *
     * Use cases:
     * - Function parameters (avoid copying arrays)
     * - View into Vector, Array, or C arrays
     * - Sub-ranges of existing data
     * - Interop with C APIs (Data() returns raw pointer)
     *
     * Example:
     * @code
     *   void ProcessData(wax::Span<const int> data) {
     *       for (int val : data) {
     *           // ...
     *       }
     *   }
     *
     *   wax::Array<int, 100> arr = {};
     *   ProcessData(arr);  // Implicit conversion
     *
     *   int raw_array[50] = {};
     *   ProcessData(raw_array);  // Works with C arrays
     * @endcode
     */
    template <typename T> class Span
    {
    public:
        using ValueType = T;
        using SizeType = size_t;
        using Iterator = T*;
        using ConstIterator = const T*;

        constexpr Span() noexcept
            : m_data{nullptr}
            , m_size{0}
        {
        }

        constexpr Span(T* data, size_t size) noexcept
            : m_data{data}
            , m_size{size}
        {
        }

        template <size_t N>
        constexpr Span(T (&array)[N]) noexcept
            : m_data{array}
            , m_size{N}
        {
        }

        template <size_t N>
        constexpr Span(wax::Array<T, N>& array) noexcept
            : m_data{array.Data()}
            , m_size{N}
        {
        }

        // Only if T is const
        template <size_t N, typename U = T>
        constexpr Span(const wax::Array<typename std::remove_const_t<U>, N>& array) noexcept
            requires std::is_const_v<T>
            : m_data{array.Data()}
            , m_size{N}
        {
        }

        constexpr Span(T* begin, T* end) noexcept
            : m_data{begin}
            , m_size{static_cast<size_t>(end - begin)}
        {
            hive::Assert(end >= begin, "Invalid iterator range");
        }

        constexpr Span(const Span&) noexcept = default;
        constexpr Span& operator=(const Span&) noexcept = default;

        // Element access (bounds-checked in debug)
        [[nodiscard]] constexpr T& operator[](size_t index) noexcept
        {
            hive::Assert(index < m_size, "Span index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr const T& operator[](size_t index) const noexcept
        {
            hive::Assert(index < m_size, "Span index out of bounds");
            return m_data[index];
        }

        // Element access (always bounds-checked)
        [[nodiscard]] constexpr T& At(size_t index)
        {
            hive::Check(index < m_size, "Span index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr const T& At(size_t index) const
        {
            hive::Check(index < m_size, "Span index out of bounds");
            return m_data[index];
        }

        // First and last element access
        [[nodiscard]] constexpr T& Front() noexcept
        {
            hive::Assert(m_size > 0, "Span is empty");
            return m_data[0];
        }

        [[nodiscard]] constexpr const T& Front() const noexcept
        {
            hive::Assert(m_size > 0, "Span is empty");
            return m_data[0];
        }

        [[nodiscard]] constexpr T& Back() noexcept
        {
            hive::Assert(m_size > 0, "Span is empty");
            return m_data[m_size - 1];
        }

        [[nodiscard]] constexpr const T& Back() const noexcept
        {
            hive::Assert(m_size > 0, "Span is empty");
            return m_data[m_size - 1];
        }

        // Raw data access
        [[nodiscard]] constexpr T* Data() noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr const T* Data() const noexcept
        {
            return m_data;
        }

        // Size information
        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return m_size;
        }

        [[nodiscard]] constexpr size_t SizeBytes() const noexcept
        {
            return m_size * sizeof(T);
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return m_size == 0;
        }

        // Iterator support
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
            return m_data + m_size;
        }

        [[nodiscard]] constexpr ConstIterator End() const noexcept
        {
            return m_data + m_size;
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

        // Subspan operations
        [[nodiscard]] constexpr Span<T> First(size_t count) const noexcept
        {
            hive::Assert(count <= m_size, "Count exceeds span size");
            return Span<T>{m_data, count};
        }

        [[nodiscard]] constexpr Span<T> Last(size_t count) const noexcept
        {
            hive::Assert(count <= m_size, "Count exceeds span size");
            return Span<T>{m_data + (m_size - count), count};
        }

        [[nodiscard]] constexpr Span<T> Subspan(size_t offset, size_t count) const noexcept
        {
            hive::Assert(offset <= m_size, "Offset exceeds span size");
            hive::Assert(offset + count <= m_size, "Subspan exceeds span bounds");
            return Span<T>{m_data + offset, count};
        }

        [[nodiscard]] constexpr Span<T> Subspan(size_t offset) const noexcept
        {
            hive::Assert(offset <= m_size, "Offset exceeds span size");
            return Span<T>{m_data + offset, m_size - offset};
        }

    private:
        T* m_data;
        size_t m_size;
    };

    // Deduction guides
    template <typename T, size_t N> Span(T (&)[N]) -> Span<T>;

    template <typename T, size_t N> Span(wax::Array<T, N>&) -> Span<T>;

    template <typename T, size_t N> Span(const wax::Array<T, N>&) -> Span<const T>;
} // namespace wax
