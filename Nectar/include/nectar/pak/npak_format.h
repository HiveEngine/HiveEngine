#pragma once

#include <nectar/core/content_hash.h>
#include <cstddef>
#include <cstdint>

namespace nectar
{
    constexpr uint32_t kNpakMagic = 0x4B41504E; // "NPAK" little-endian
    constexpr uint32_t kNpakVersion = 1;
    constexpr size_t kBlockSize = 65536;         // 64KB decompressed block
    constexpr size_t kBlockAlignment = 4096;     // 4KB file alignment

    /// Sentinel hash used to store the AssetManifest inside the .npak
    constexpr ContentHash kManifestSentinel{0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};

    enum class CompressionMethod : uint8_t
    {
        None = 0,
        LZ4  = 1,
        Zstd = 2,
    };

    /// .npak file header — always first 32 bytes.
    struct NpakHeader
    {
        uint32_t magic;
        uint32_t version;
        uint32_t flags;
        uint32_t block_count;
        uint64_t toc_offset;
        uint32_t toc_size;
        uint32_t toc_crc32;
    };
    static_assert(sizeof(NpakHeader) == 32);

    /// Per-asset entry in the TOC. Sorted by content_hash for binary search.
    #pragma pack(push, 1)
    struct NpakAssetEntry
    {
        ContentHash content_hash;    // 16 bytes
        uint32_t first_block;
        uint32_t offset_in_block;    // byte offset within the first block
        uint32_t uncompressed_size;
    };
    #pragma pack(pop)
    static_assert(sizeof(NpakAssetEntry) == 28);

    /// Per-block entry in the TOC.
    #pragma pack(push, 1)
    struct NpakBlockEntry
    {
        uint64_t file_offset;
        uint32_t compressed_size;
        CompressionMethod compression;  // 1 byte
    };
    #pragma pack(pop)
    static_assert(sizeof(NpakBlockEntry) == 13);
}
