#pragma once

#include <nectar/core/content_hash.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>
#include <comb/default_allocator.h>
#include <cstdint>
#include <cstring>

namespace nectar
{
    /// Header prepended to intermediate/cooked blobs for validation.
    /// magic identifies the asset type (e.g. 'NTEX', 'NMSH').
    /// format_version tracks the blob format — bump on breaking changes.
    #pragma pack(push, 1)
    struct AssetBlobHeader
    {
        uint32_t magic;
        uint16_t format_version;
        uint16_t flags;           // reserved, must be 0
        ContentHash content_hash; // hash of the payload (excludes this header)
    };
    #pragma pack(pop)

    static_assert(sizeof(AssetBlobHeader) == 24, "AssetBlobHeader must be 24 bytes packed");

    /// Write header + payload into a single buffer.
    inline wax::ByteBuffer<> WriteBlob(uint32_t magic, uint16_t format_version,
                                        wax::ByteSpan payload, comb::DefaultAllocator& alloc)
    {
        wax::ByteBuffer<> buf{alloc, sizeof(AssetBlobHeader) + payload.Size()};

        AssetBlobHeader header{};
        header.magic = magic;
        header.format_version = format_version;
        header.flags = 0;
        header.content_hash = ContentHash::FromData(payload);

        buf.Append(&header, sizeof(header));
        buf.Append(payload.Data(), payload.Size());
        return buf;
    }

    /// Validate header and return payload span.
    /// Returns empty span if blob is too small or magic doesn't match.
    inline wax::ByteSpan ReadBlob(wax::ByteSpan blob, uint32_t expected_magic)
    {
        if (blob.Size() < sizeof(AssetBlobHeader))
            return wax::ByteSpan{static_cast<const uint8_t*>(nullptr), size_t{0}};

        AssetBlobHeader header{};
        std::memcpy(&header, blob.Data(), sizeof(header));

        if (header.magic != expected_magic)
            return wax::ByteSpan{static_cast<const uint8_t*>(nullptr), size_t{0}};

        wax::ByteSpan payload{blob.Data() + sizeof(AssetBlobHeader),
                              blob.Size() - sizeof(AssetBlobHeader)};

        ContentHash actual = ContentHash::FromData(payload);
        if (!(actual == header.content_hash))
            return wax::ByteSpan{static_cast<const uint8_t*>(nullptr), size_t{0}};

        return payload;
    }
}
