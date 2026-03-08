#pragma once

#include <nectar/core/content_hash.h>

#include <cstddef>
#include <cstdint>

namespace nectar
{
    constexpr uint32_t kNpakMagic = 0x4B41504E; // "NPAK" little-endian
    constexpr uint32_t kNpakVersion = 1;
    constexpr size_t kBlockSize = 65536;     // 64KB decompressed block
    constexpr size_t kBlockAlignment = 4096; // 4KB file alignment

    /// Sentinel hash used to store the AssetManifest inside the .npak
    constexpr ContentHash kManifestSentinel{0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};

    enum class CompressionMethod : uint8_t
    {
        NONE = 0,
        LZ4 = 1,
        ZSTD = 2,
    };

    /// .npak file header — always first 32 bytes.
    struct NpakHeader
    {
        uint32_t m_magic;
        uint32_t m_version;
        uint32_t m_flags;
        uint32_t m_blockCount;
        uint64_t m_tocOffset;
        uint32_t m_tocSize;
        uint32_t m_tocCrc32;
    };
    static_assert(sizeof(NpakHeader) == 32);

/// Per-asset entry in the TOC. Sorted by content_hash for binary search.
#pragma pack(push, 1)
    struct NpakAssetEntry
    {
        ContentHash m_contentHash; // 16 bytes
        uint32_t m_firstBlock;
        uint32_t m_offsetInBlock; // byte offset within the first block
        uint32_t m_uncompressedSize;
    };
#pragma pack(pop)
    static_assert(sizeof(NpakAssetEntry) == 28);

/// Per-block entry in the TOC.
#pragma pack(push, 1)
    struct NpakBlockEntry
    {
        uint64_t m_fileOffset;
        uint32_t m_compressedSize;
        CompressionMethod m_compression; // 1 byte
    };
#pragma pack(pop)
    static_assert(sizeof(NpakBlockEntry) == 13);
} // namespace nectar
