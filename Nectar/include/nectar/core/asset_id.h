#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <hive/core/assert.h>

namespace nectar
{
    /**
     * Fixed-size string for AssetId string representation
     *
     * Simple stack-allocated string with exact capacity for hex UUID (32 chars).
     */
    class AssetIdString
    {
    public:
        static constexpr size_t kCapacity = 32;

        constexpr AssetIdString() noexcept
            : buffer_{}
            , size_{0}
        {
            buffer_[0] = '\0';
        }

        constexpr AssetIdString(const char* str, size_t len) noexcept
            : buffer_{}
            , size_{0}
        {
            size_t copy_len = len <= kCapacity ? len : kCapacity;
            for (size_t i = 0; i < copy_len; ++i)
            {
                buffer_[i] = str[i];
            }
            buffer_[copy_len] = '\0';
            size_ = copy_len;
        }

        [[nodiscard]] constexpr const char* CStr() const noexcept { return buffer_; }
        [[nodiscard]] constexpr size_t Size() const noexcept { return size_; }

        [[nodiscard]] constexpr bool operator==(const AssetIdString& other) const noexcept
        {
            if (size_ != other.size_) return false;
            for (size_t i = 0; i < size_; ++i)
            {
                if (buffer_[i] != other.buffer_[i]) return false;
            }
            return true;
        }

    private:
        char buffer_[kCapacity + 1];
        size_t size_;
    };

    /**
     * 128-bit unique asset identifier
     *
     * AssetId provides globally unique identification for assets across projects
     * and time. It's implemented as two 64-bit integers for efficient storage
     * and comparison while maintaining uniqueness comparable to UUIDs.
     *
     * Performance characteristics:
     * - Storage: 16 bytes (two uint64)
     * - Comparison: O(1) - two 64-bit compares
     * - Hash: O(1) - XOR of high and low
     * - ToString: O(n) - where n = 32 hex chars
     * - Generate: Platform-dependent (crypto-random preferred)
     *
     * Limitations:
     * - Generation requires platform-specific random source
     * - Not sortable by creation time (use separate timestamp)
     * - String representation is 32 hex characters
     *
     * Use cases:
     * - Asset database primary keys
     * - Cross-reference between assets (dependencies)
     * - Persistent identification across reimport
     * - Network replication of asset references
     *
     * Example:
     * @code
     *   nectar::AssetId id = nectar::AssetId::Generate();
     *   auto str = id.ToString();  // "a1b2c3d4e5f6789012345678abcdef00"
     *
     *   nectar::AssetId parsed = nectar::AssetId::FromString(str.CStr());
     *   assert(id == parsed);
     * @endcode
     */
    class AssetId
    {
    public:
        static constexpr size_t kByteSize = 16;
        static constexpr size_t kStringLength = 32;

        constexpr AssetId() noexcept
            : high_{0}
            , low_{0}
        {}

        constexpr AssetId(uint64_t high, uint64_t low) noexcept
            : high_{high}
            , low_{low}
        {}

        [[nodiscard]] static constexpr AssetId Invalid() noexcept
        {
            return AssetId{0, 0};
        }

        [[nodiscard]] static AssetId Generate() noexcept;

        [[nodiscard]] static AssetId FromString(const char* str, size_t length) noexcept;

        [[nodiscard]] static AssetId FromString(const char* str) noexcept
        {
            return FromString(str, std::strlen(str));
        }

        [[nodiscard]] static AssetId FromBytes(const uint8_t* bytes) noexcept
        {
            hive::Assert(bytes != nullptr, "Bytes cannot be null");

            uint64_t high = 0;
            uint64_t low = 0;

            for (size_t i = 0; i < 8; ++i)
            {
                high |= static_cast<uint64_t>(bytes[i]) << (56 - i * 8);
            }
            for (size_t i = 0; i < 8; ++i)
            {
                low |= static_cast<uint64_t>(bytes[8 + i]) << (56 - i * 8);
            }

            return AssetId{high, low};
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return high_ != 0 || low_ != 0;
        }

        [[nodiscard]] constexpr uint64_t High() const noexcept
        {
            return high_;
        }

        [[nodiscard]] constexpr uint64_t Low() const noexcept
        {
            return low_;
        }

        [[nodiscard]] AssetIdString ToString() const noexcept
        {
            constexpr char kHexChars[] = "0123456789abcdef";

            char buffer[kStringLength + 1];

            for (size_t i = 0; i < 8; ++i)
            {
                uint8_t byte = static_cast<uint8_t>(high_ >> (56 - i * 8));
                buffer[i * 2] = kHexChars[byte >> 4];
                buffer[i * 2 + 1] = kHexChars[byte & 0x0F];
            }
            for (size_t i = 0; i < 8; ++i)
            {
                uint8_t byte = static_cast<uint8_t>(low_ >> (56 - i * 8));
                buffer[16 + i * 2] = kHexChars[byte >> 4];
                buffer[16 + i * 2 + 1] = kHexChars[byte & 0x0F];
            }
            buffer[kStringLength] = '\0';

            return AssetIdString{buffer, kStringLength};
        }

        void ToBytes(uint8_t* out) const noexcept
        {
            hive::Assert(out != nullptr, "Output buffer cannot be null");

            for (size_t i = 0; i < 8; ++i)
            {
                out[i] = static_cast<uint8_t>(high_ >> (56 - i * 8));
            }
            for (size_t i = 0; i < 8; ++i)
            {
                out[8 + i] = static_cast<uint8_t>(low_ >> (56 - i * 8));
            }
        }

        [[nodiscard]] constexpr size_t Hash() const noexcept
        {
            return static_cast<size_t>(high_ ^ low_);
        }

        [[nodiscard]] constexpr bool operator==(const AssetId& other) const noexcept
        {
            return high_ == other.high_ && low_ == other.low_;
        }

        [[nodiscard]] constexpr bool operator!=(const AssetId& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr bool operator<(const AssetId& other) const noexcept
        {
            if (high_ != other.high_)
            {
                return high_ < other.high_;
            }
            return low_ < other.low_;
        }

        [[nodiscard]] constexpr bool operator<=(const AssetId& other) const noexcept
        {
            return !(other < *this);
        }

        [[nodiscard]] constexpr bool operator>(const AssetId& other) const noexcept
        {
            return other < *this;
        }

        [[nodiscard]] constexpr bool operator>=(const AssetId& other) const noexcept
        {
            return !(*this < other);
        }

    private:
        uint64_t high_;
        uint64_t low_;
    };

}

// Hash specialization for wax::HashMap/HashSet
template<>
struct std::hash<nectar::AssetId>
{
    size_t operator()(const nectar::AssetId& id) const noexcept
    {
        return id.Hash();
    }
};
