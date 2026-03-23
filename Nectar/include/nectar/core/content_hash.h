#pragma once

#include <hive/hive_config.h>

#include <wax/serialization/byte_span.h>

#include <cstddef>
#include <cstdint>
#include <functional>

namespace nectar
{
    /// Fixed-capacity hex string (32 chars) for 128-bit hash display.
    class HexString32
    {
    public:
        static constexpr size_t kCapacity = 32;

        constexpr HexString32() noexcept
            : m_buffer{}
            , m_size{0}
        {
            m_buffer[0] = '\0';
        }

        constexpr HexString32(const char* str, size_t len) noexcept
            : m_buffer{}
            , m_size{0}
        {
            size_t n = len <= kCapacity ? len : kCapacity;
            for (size_t i = 0; i < n; ++i)
                m_buffer[i] = str[i];
            m_buffer[n] = '\0';
            m_size = n;
        }

        [[nodiscard]] constexpr const char* CStr() const noexcept
        {
            return m_buffer;
        }
        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return m_size;
        }

    private:
        char m_buffer[kCapacity + 1];
        size_t m_size;
    };

    /// 128-bit content hash for asset data identity (CAS key).
    /// Uses FNV-1a internally for now — will swap to Blake3 later.
    class ContentHash
    {
    public:
        constexpr ContentHash() noexcept
            : m_high{0}
            , m_low{0}
        {
        }

        constexpr ContentHash(uint64_t high, uint64_t low) noexcept
            : m_high{high}
            , m_low{low}
        {
        }

        [[nodiscard]] static constexpr ContentHash Invalid() noexcept
        {
            return ContentHash{0, 0};
        }

        /// Hash a blob of data
        [[nodiscard]] HIVE_API static ContentHash FromData(const void* data, size_t size) noexcept;

        [[nodiscard]] static ContentHash FromData(wax::ByteSpan span) noexcept
        {
            return FromData(span.Data(), span.Size());
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_high != 0 || m_low != 0;
        }

        [[nodiscard]] constexpr uint64_t High() const noexcept
        {
            return m_high;
        }
        [[nodiscard]] constexpr uint64_t Low() const noexcept
        {
            return m_low;
        }

        [[nodiscard]] constexpr size_t Hash() const noexcept
        {
            return static_cast<size_t>(m_high ^ m_low);
        }

        [[nodiscard]] HexString32 ToString() const noexcept
        {
            constexpr char kHex[] = "0123456789abcdef";
            char buf[33];

            for (size_t i = 0; i < 8; ++i)
            {
                uint8_t byte = static_cast<uint8_t>(m_high >> (56 - i * 8));
                buf[i * 2] = kHex[byte >> 4];
                buf[i * 2 + 1] = kHex[byte & 0x0F];
            }
            for (size_t i = 0; i < 8; ++i)
            {
                uint8_t byte = static_cast<uint8_t>(m_low >> (56 - i * 8));
                buf[16 + i * 2] = kHex[byte >> 4];
                buf[16 + i * 2 + 1] = kHex[byte & 0x0F];
            }
            buf[32] = '\0';

            return HexString32{buf, 32};
        }

        [[nodiscard]] constexpr bool operator==(const ContentHash& other) const noexcept
        {
            return m_high == other.m_high && m_low == other.m_low;
        }

        [[nodiscard]] constexpr bool operator!=(const ContentHash& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr bool operator<(const ContentHash& other) const noexcept
        {
            if (m_high != other.m_high)
                return m_high < other.m_high;
            return m_low < other.m_low;
        }

    private:
        uint64_t m_high;
        uint64_t m_low;
    };
} // namespace nectar

template <> struct std::hash<nectar::ContentHash>
{
    size_t operator()(const nectar::ContentHash& h) const noexcept
    {
        return h.Hash();
    }
};
