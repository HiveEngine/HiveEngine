#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <hive/core/assert.h>

namespace wax
{
    /**
     * Non-owning view over binary data
     *
     * ByteSpan provides a lightweight reference to an existing byte buffer
     * without owning the data. It's similar to std::span<const uint8_t> but
     * with additional utilities for reading structured data.
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
     * - Read-only (cannot modify underlying data)
     * - No memory management
     * - Dangling pointer risk if source is destroyed
     *
     * Use cases:
     * - Reading binary file contents without copying
     * - Parsing structured data from byte streams
     * - Passing binary data to functions
     * - Sub-ranges of existing buffers
     *
     * Example:
     * @code
     *   void ProcessData(wax::ByteSpan data) {
     *       uint32_t magic = data.Read<uint32_t>(0);
     *       uint32_t version = data.Read<uint32_t>(4);
     *       auto payload = data.Subspan(8, data.Size() - 8);
     *   }
     * @endcode
     */
    class ByteSpan
    {
    public:
        constexpr ByteSpan() noexcept
            : data_{nullptr}
            , size_{0}
        {}

        constexpr ByteSpan(const uint8_t* data, size_t size) noexcept
            : data_{data}
            , size_{size}
        {}

        constexpr ByteSpan(const void* data, size_t size) noexcept
            : data_{static_cast<const uint8_t*>(data)}
            , size_{size}
        {}

        template<size_t N>
        constexpr ByteSpan(const uint8_t (&array)[N]) noexcept
            : data_{array}
            , size_{N}
        {}

        [[nodiscard]] constexpr const uint8_t* Data() const noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] constexpr uint8_t operator[](size_t index) const noexcept
        {
            hive::Assert(index < size_, "ByteSpan index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr uint8_t At(size_t index) const
        {
            hive::Check(index < size_, "ByteSpan index out of bounds");
            return data_[index];
        }

        template<typename T>
        [[nodiscard]] T Read(size_t offset) const noexcept
        {
            hive::Assert(offset + sizeof(T) <= size_, "ByteSpan read out of bounds");

            T result;
            std::memcpy(&result, data_ + offset, sizeof(T));
            return result;
        }

        template<typename T>
        [[nodiscard]] bool TryRead(size_t offset, T& out) const noexcept
        {
            if (offset + sizeof(T) > size_)
            {
                return false;
            }

            std::memcpy(&out, data_ + offset, sizeof(T));
            return true;
        }

        [[nodiscard]] constexpr ByteSpan Subspan(size_t offset, size_t count) const noexcept
        {
            hive::Assert(offset <= size_, "ByteSpan subspan offset out of bounds");
            hive::Assert(offset + count <= size_, "ByteSpan subspan exceeds bounds");
            return ByteSpan{data_ + offset, count};
        }

        [[nodiscard]] constexpr ByteSpan Subspan(size_t offset) const noexcept
        {
            hive::Assert(offset <= size_, "ByteSpan subspan offset out of bounds");
            return ByteSpan{data_ + offset, size_ - offset};
        }

        [[nodiscard]] constexpr ByteSpan First(size_t count) const noexcept
        {
            hive::Assert(count <= size_, "ByteSpan first count exceeds size");
            return ByteSpan{data_, count};
        }

        [[nodiscard]] constexpr ByteSpan Last(size_t count) const noexcept
        {
            hive::Assert(count <= size_, "ByteSpan last count exceeds size");
            return ByteSpan{data_ + (size_ - count), count};
        }

        [[nodiscard]] constexpr const uint8_t* begin() const noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr const uint8_t* end() const noexcept
        {
            return data_ + size_;
        }

        [[nodiscard]] bool operator==(const ByteSpan& other) const noexcept
        {
            if (size_ != other.size_)
            {
                return false;
            }
            if (data_ == other.data_)
            {
                return true;
            }
            return std::memcmp(data_, other.data_, size_) == 0;
        }

        [[nodiscard]] bool operator!=(const ByteSpan& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        const uint8_t* data_;
        size_t size_;
    };

}
