#include <nectar/pak/pak_builder.h>

#include <hive/profiling/profiler.h>

#include <nectar/pak/compression.h>
#include <nectar/pak/crc32.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace nectar
{
    PakBuilder::PakBuilder(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_entries{alloc}
    {
    }

    void PakBuilder::AddBlob(ContentHash hash, wax::ByteSpan data, CompressionMethod compression)
    {
        BuildEntry entry;
        entry.m_hash = hash;
        entry.m_data = wax::ByteBuffer{*m_alloc};
        entry.m_data.Append(data);
        entry.m_compression = compression;
        m_entries.PushBack(static_cast<BuildEntry&&>(entry));
    }

    void PakBuilder::SetManifest(const AssetManifest& manifest)
    {
        m_manifest = &manifest;
    }

    bool PakBuilder::Build(wax::StringView outputPath)
    {
        HIVE_PROFILE_SCOPE_N("PakBuilder::Build");
        // Build path string with null terminator
        wax::String path{*m_alloc};
        path.Append(outputPath.Data(), outputPath.Size());

        FILE* file = std::fopen(path.CStr(), "wb");
        if (!file)
            return false;

        // Write placeholder header
        NpakHeader header{};
        header.m_magic = kNpakMagic;
        header.m_version = kNpakVersion;
        header.m_flags = 0;
        std::fwrite(&header, sizeof(NpakHeader), 1, file);

        // If manifest is set, add it as a blob at sentinel hash
        wax::ByteBuffer manifestData{*m_alloc};
        if (m_manifest)
        {
            manifestData = m_manifest->Serialize(*m_alloc);
            BuildEntry me;
            me.m_hash = kManifestSentinel;
            me.m_data = wax::ByteBuffer{*m_alloc};
            me.m_data.Append(manifestData.View());
            me.m_compression = CompressionMethod::NONE;
            m_entries.PushBack(static_cast<BuildEntry&&>(me));
        }

        // Collect TOC data as we write blocks
        wax::Vector<NpakAssetEntry> assetEntries{*m_alloc};
        wax::Vector<NpakBlockEntry> blockEntries{*m_alloc};

        for (size_t i = 0; i < m_entries.Size(); ++i)
        {
            auto& entry = m_entries[i];
            const uint8_t* src = entry.m_data.Data();
            size_t remaining = entry.m_data.Size();

            NpakAssetEntry ae{};
            ae.m_contentHash = entry.m_hash;
            ae.m_firstBlock = static_cast<uint32_t>(blockEntries.Size());
            ae.m_uncompressedSize = static_cast<uint32_t>(remaining);

            // For the first block of this asset, compute offset_in_block.
            // Each asset starts at the beginning of its first block (offset 0).
            ae.m_offsetInBlock = 0;

            // Split data into 64KB chunks and compress each
            while (remaining > 0)
            {
                size_t chunkSize = remaining < kBlockSize ? remaining : kBlockSize;
                wax::ByteSpan chunk{src, chunkSize};

                // Pad file position to 4KB alignment
                long pos = std::ftell(file);
                size_t aligned = (static_cast<size_t>(pos) + kBlockAlignment - 1) & ~(kBlockAlignment - 1);
                if (static_cast<size_t>(pos) < aligned)
                {
                    size_t padding = aligned - static_cast<size_t>(pos);
                    // Write zero padding
                    uint8_t zeros[4096]{};
                    while (padding > 0)
                    {
                        size_t toWrite = padding < sizeof(zeros) ? padding : sizeof(zeros);
                        std::fwrite(zeros, 1, toWrite, file);
                        padding -= toWrite;
                    }
                }

                uint64_t blockOffset = static_cast<uint64_t>(std::ftell(file));

                // Try to compress
                auto compressed = Compress(chunk, entry.m_compression, *m_alloc);

                NpakBlockEntry be{};
                be.m_fileOffset = blockOffset;

                if (compressed.Size() > 0)
                {
                    // Compression succeeded and was beneficial
                    be.m_compressedSize = static_cast<uint32_t>(compressed.Size());
                    be.m_compression = entry.m_compression;
                    std::fwrite(compressed.Data(), 1, compressed.Size(), file);
                }
                else
                {
                    be.m_compressedSize = static_cast<uint32_t>(chunkSize);
                    be.m_compression = CompressionMethod::NONE;
                    std::fwrite(chunk.Data(), 1, chunkSize, file);
                }

                blockEntries.PushBack(be);
                src += chunkSize;
                remaining -= chunkSize;
            }

            assetEntries.PushBack(ae);
        }

        // Sort asset entries by ContentHash for binary search
        // Simple insertion sort (sufficient for archive building)
        for (size_t i = 1; i < assetEntries.Size(); ++i)
        {
            NpakAssetEntry tmp = assetEntries[i];
            size_t j = i;
            while (j > 0 && tmp.m_contentHash < assetEntries[j - 1].m_contentHash)
            {
                assetEntries[j] = assetEntries[j - 1];
                --j;
            }
            assetEntries[j] = tmp;
        }

        // Write TOC
        uint64_t tocOffset = static_cast<uint64_t>(std::ftell(file));

        // TOC layout: [asset_count u32] [asset entries...] [block_count u32] [block entries...]
        uint32_t assetCount = static_cast<uint32_t>(assetEntries.Size());
        uint32_t blockCount = static_cast<uint32_t>(blockEntries.Size());

        // Build TOC buffer for CRC32
        size_t tocSize = sizeof(uint32_t) + assetCount * sizeof(NpakAssetEntry) + sizeof(uint32_t) +
                         blockCount * sizeof(NpakBlockEntry);
        wax::ByteBuffer tocBuf{*m_alloc};
        tocBuf.Resize(tocSize);
        uint8_t* tocPtr = tocBuf.Data();

        std::memcpy(tocPtr, &assetCount, sizeof(uint32_t));
        tocPtr += sizeof(uint32_t);
        if (assetCount > 0)
        {
            std::memcpy(tocPtr, &assetEntries[0], assetCount * sizeof(NpakAssetEntry));
            tocPtr += assetCount * sizeof(NpakAssetEntry);
        }

        std::memcpy(tocPtr, &blockCount, sizeof(uint32_t));
        tocPtr += sizeof(uint32_t);
        if (blockCount > 0)
        {
            std::memcpy(tocPtr, &blockEntries[0], blockCount * sizeof(NpakBlockEntry));
        }

        std::fwrite(tocBuf.Data(), 1, tocSize, file);

        // Finalize header
        header.m_blockCount = blockCount;
        header.m_tocOffset = tocOffset;
        header.m_tocSize = static_cast<uint32_t>(tocSize);
        header.m_tocCrc32 = Crc32(tocBuf.Data(), tocSize);

        std::fseek(file, 0, SEEK_SET);
        std::fwrite(&header, sizeof(NpakHeader), 1, file);

        std::fclose(file);
        return true;
    }
} // namespace nectar
