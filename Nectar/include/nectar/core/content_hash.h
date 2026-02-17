#pragma once

#include <nectar/core/asset_id.h>
#include <wax/serialization/byte_span.h>
#include <cstdint>
#include <cstddef>

namespace nectar
{
    /// 128-bit content hash for asset data identity (CAS key).
    /// Uses FNV-1a internally for now — will swap to Blake3 later.
    class ContentHash
    {
    public:
        constexpr ContentHash() noexcept
            : high_{0}
            , low_{0}
        {}

        constexpr ContentHash(uint64_t high, uint64_t low) noexcept
            : high_{high}
            , low_{low}
        {}

        [[nodiscard]] static constexpr ContentHash Invalid() noexcept
        {
            return ContentHash{0, 0};
        }

        /// Hash a blob of data
        [[nodiscard]] static ContentHash FromData(const void* data, size_t size) noexcept;

        [[nodiscard]] static ContentHash FromData(wax::ByteSpan span) noexcept
        {
            return FromData(span.Data(), span.Size());
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return high_ != 0 || low_ != 0;
        }

        [[nodiscard]] constexpr uint64_t High() const noexcept { return high_; }
        [[nodiscard]] constexpr uint64_t Low() const noexcept { return low_; }

        [[nodiscard]] constexpr size_t Hash() const noexcept
        {
            return static_cast<size_t>(high_ ^ low_);
        }

        [[nodiscard]] AssetIdString ToString() const noexcept
        {
            constexpr char kHex[] = "0123456789abcdef";
            char buf[33];

            for (size_t i = 0; i < 8; ++i)
            {
                uint8_t byte = static_cast<uint8_t>(high_ >> (56 - i * 8));
                buf[i * 2] = kHex[byte >> 4];
                buf[i * 2 + 1] = kHex[byte & 0x0F];
            }
            for (size_t i = 0; i < 8; ++i)
            {
                uint8_t byte = static_cast<uint8_t>(low_ >> (56 - i * 8));
                buf[16 + i * 2] = kHex[byte >> 4];
                buf[16 + i * 2 + 1] = kHex[byte & 0x0F];
            }
            buf[32] = '\0';

            return AssetIdString{buf, 32};
        }

        [[nodiscard]] constexpr bool operator==(const ContentHash& other) const noexcept
        {
            return high_ == other.high_ && low_ == other.low_;
        }

        [[nodiscard]] constexpr bool operator!=(const ContentHash& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr bool operator<(const ContentHash& other) const noexcept
        {
            if (high_ != other.high_) return high_ < other.high_;
            return low_ < other.low_;
        }

    private:
        uint64_t high_;
        uint64_t low_;
    };
}

template<>
struct std::hash<nectar::ContentHash>
{
    size_t operator()(const nectar::ContentHash& h) const noexcept
    {
        return h.Hash();
    }
};
