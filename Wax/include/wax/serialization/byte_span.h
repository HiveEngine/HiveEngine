#pragma once

#include <hive/core/assert.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wax
{
    // Non-owning, read-only view over binary data (const uint8_t* + size).
    class ByteSpan
    {
    public:
        constexpr ByteSpan() noexcept
            : m_data{nullptr}
            , m_size{0}
        {
        }

        constexpr ByteSpan(const uint8_t* data, size_t size) noexcept
            : m_data{data}
            , m_size{size}
        {
        }

        constexpr ByteSpan(const void* data, size_t size) noexcept
            : m_data{static_cast<const uint8_t*>(data)}
            , m_size{size}
        {
        }

        template <size_t N>
        constexpr ByteSpan(const uint8_t (&array)[N]) noexcept
            : m_data{array}
            , m_size{N}
        {
        }

        [[nodiscard]] constexpr const uint8_t* Data() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return m_size;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return m_size == 0;
        }

        [[nodiscard]] constexpr uint8_t operator[](size_t index) const noexcept
        {
            hive::Assert(index < m_size, "ByteSpan index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr uint8_t At(size_t index) const
        {
            hive::Check(index < m_size, "ByteSpan index out of bounds");
            return m_data[index];
        }

        template <typename T> [[nodiscard]] T Read(size_t offset) const noexcept
        {
            hive::Assert(offset + sizeof(T) <= m_size, "ByteSpan read out of bounds");

            T result;
            std::memcpy(&result, m_data + offset, sizeof(T));
            return result;
        }

        template <typename T> [[nodiscard]] bool TryRead(size_t offset, T& out) const noexcept
        {
            if (offset + sizeof(T) > m_size)
            {
                return false;
            }

            std::memcpy(&out, m_data + offset, sizeof(T));
            return true;
        }

        [[nodiscard]] constexpr ByteSpan Subspan(size_t offset, size_t count) const noexcept
        {
            hive::Assert(offset <= m_size, "ByteSpan subspan offset out of bounds");
            hive::Assert(offset + count <= m_size, "ByteSpan subspan exceeds bounds");
            return ByteSpan{m_data + offset, count};
        }

        [[nodiscard]] constexpr ByteSpan Subspan(size_t offset) const noexcept
        {
            hive::Assert(offset <= m_size, "ByteSpan subspan offset out of bounds");
            return ByteSpan{m_data + offset, m_size - offset};
        }

        [[nodiscard]] constexpr ByteSpan First(size_t count) const noexcept
        {
            hive::Assert(count <= m_size, "ByteSpan first count exceeds size");
            return ByteSpan{m_data, count};
        }

        [[nodiscard]] constexpr ByteSpan Last(size_t count) const noexcept
        {
            hive::Assert(count <= m_size, "ByteSpan last count exceeds size");
            return ByteSpan{m_data + (m_size - count), count};
        }

        [[nodiscard]] constexpr const uint8_t* Begin() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr const uint8_t* End() const noexcept
        {
            return m_data + m_size;
        }

        [[nodiscard]] bool operator==(const ByteSpan& other) const noexcept
        {
            if (m_size != other.m_size)
            {
                return false;
            }
            if (m_data == other.m_data)
            {
                return true;
            }
            return std::memcmp(m_data, other.m_data, m_size) == 0;
        }

        [[nodiscard]] bool operator!=(const ByteSpan& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        const uint8_t* m_data;
        size_t m_size;
    };

} // namespace wax
