#pragma once

#include <cstddef>
#include <hive/core/assert.h>
#include <wax/containers/array.h>

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
    template<typename T>
    class Span
    {
    public:
        using ValueType = T;
        using SizeType = size_t;
        using Iterator = T*;
        using ConstIterator = const T*;

        constexpr Span() noexcept
            : data_{nullptr}
            , size_{0}
        {}

        constexpr Span(T* data, size_t size) noexcept
            : data_{data}
            , size_{size}
        {}

        template<size_t N>
        constexpr Span(T (&array)[N]) noexcept
            : data_{array}
            , size_{N}
        {}

        template<size_t N>
        constexpr Span(wax::Array<T, N>& array) noexcept
            : data_{array.Data()}
            , size_{N}
        {}

        // Only if T is const
        template<size_t N, typename U = T>
        constexpr Span(const wax::Array<typename std::remove_const_t<U>, N>& array) noexcept
            requires std::is_const_v<T>
            : data_{array.Data()}
            , size_{N}
        {}

        constexpr Span(T* begin, T* end) noexcept
            : data_{begin}
            , size_{static_cast<size_t>(end - begin)}
        {
            hive::Assert(end >= begin, "Invalid iterator range");
        }

        constexpr Span(const Span&) noexcept = default;
        constexpr Span& operator=(const Span&) noexcept = default;

        // Element access (bounds-checked in debug)
        [[nodiscard]] constexpr T& operator[](size_t index) noexcept
        {
            hive::Assert(index < size_, "Span index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr const T& operator[](size_t index) const noexcept
        {
            hive::Assert(index < size_, "Span index out of bounds");
            return data_[index];
        }

        // Element access (always bounds-checked)
        [[nodiscard]] constexpr T& At(size_t index)
        {
            hive::Check(index < size_, "Span index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr const T& At(size_t index) const
        {
            hive::Check(index < size_, "Span index out of bounds");
            return data_[index];
        }

        // First and last element access
        [[nodiscard]] constexpr T& Front() noexcept
        {
            hive::Assert(size_ > 0, "Span is empty");
            return data_[0];
        }

        [[nodiscard]] constexpr const T& Front() const noexcept
        {
            hive::Assert(size_ > 0, "Span is empty");
            return data_[0];
        }

        [[nodiscard]] constexpr T& Back() noexcept
        {
            hive::Assert(size_ > 0, "Span is empty");
            return data_[size_ - 1];
        }

        [[nodiscard]] constexpr const T& Back() const noexcept
        {
            hive::Assert(size_ > 0, "Span is empty");
            return data_[size_ - 1];
        }

        // Raw data access
        [[nodiscard]] constexpr T* Data() noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr const T* Data() const noexcept
        {
            return data_;
        }

        // Size information
        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] constexpr size_t SizeBytes() const noexcept
        {
            return size_ * sizeof(T);
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return size_ == 0;
        }

        // Iterator support
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
            return data_ + size_;
        }

        [[nodiscard]] constexpr ConstIterator end() const noexcept
        {
            return data_ + size_;
        }

        // Subspan operations
        [[nodiscard]] constexpr Span<T> First(size_t count) const noexcept
        {
            hive::Assert(count <= size_, "Count exceeds span size");
            return Span<T>{data_, count};
        }

        [[nodiscard]] constexpr Span<T> Last(size_t count) const noexcept
        {
            hive::Assert(count <= size_, "Count exceeds span size");
            return Span<T>{data_ + (size_ - count), count};
        }

        [[nodiscard]] constexpr Span<T> Subspan(size_t offset, size_t count) const noexcept
        {
            hive::Assert(offset <= size_, "Offset exceeds span size");
            hive::Assert(offset + count <= size_, "Subspan exceeds span bounds");
            return Span<T>{data_ + offset, count};
        }

        [[nodiscard]] constexpr Span<T> Subspan(size_t offset) const noexcept
        {
            hive::Assert(offset <= size_, "Offset exceeds span size");
            return Span<T>{data_ + offset, size_ - offset};
        }

    private:
        T* data_;
        size_t size_;
    };

    // Deduction guides
    template<typename T, size_t N>
    Span(T (&)[N]) -> Span<T>;

    template<typename T, size_t N>
    Span(wax::Array<T, N>&) -> Span<T>;

    template<typename T, size_t N>
    Span(const wax::Array<T, N>&) -> Span<const T>;
}
