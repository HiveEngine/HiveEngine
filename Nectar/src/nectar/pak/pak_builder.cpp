#define _CRT_SECURE_NO_WARNINGS
#include <nectar/pak/pak_builder.h>
#include <nectar/pak/compression.h>
#include <nectar/pak/crc32.h>
#include <hive/profiling/profiler.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace nectar
{
    PakBuilder::PakBuilder(comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , entries_{alloc}
    {}

    void PakBuilder::AddBlob(ContentHash hash, wax::ByteSpan data,
                              CompressionMethod compression)
    {
        BuildEntry entry;
        entry.hash = hash;
        entry.data = wax::ByteBuffer<>{*alloc_};
        entry.data.Append(data);
        entry.compression = compression;
        entries_.PushBack(static_cast<BuildEntry&&>(entry));
    }

    void PakBuilder::SetManifest(const AssetManifest& manifest)
    {
        manifest_ = &manifest;
    }

    bool PakBuilder::Build(wax::StringView output_path)
    {
        HIVE_PROFILE_SCOPE_N("PakBuilder::Build");
        // Build path string with null terminator
        wax::String<> path{*alloc_};
        path.Append(output_path.Data(), output_path.Size());

        FILE* file = std::fopen(path.CStr(), "wb");
        if (!file) return false;

        // Write placeholder header
        NpakHeader header{};
        header.magic = kNpakMagic;
        header.version = kNpakVersion;
        header.flags = 0;
        std::fwrite(&header, sizeof(NpakHeader), 1, file);

        // If manifest is set, add it as a blob at sentinel hash
        wax::ByteBuffer<> manifest_data{*alloc_};
        if (manifest_)
        {
            manifest_data = manifest_->Serialize(*alloc_);
            BuildEntry me;
            me.hash = kManifestSentinel;
            me.data = wax::ByteBuffer<>{*alloc_};
            me.data.Append(manifest_data.View());
            me.compression = CompressionMethod::None;
            entries_.PushBack(static_cast<BuildEntry&&>(me));
        }

        // Collect TOC data as we write blocks
        wax::Vector<NpakAssetEntry> asset_entries{*alloc_};
        wax::Vector<NpakBlockEntry> block_entries{*alloc_};

        for (size_t i = 0; i < entries_.Size(); ++i)
        {
            auto& entry = entries_[i];
            const uint8_t* src = entry.data.Data();
            size_t remaining = entry.data.Size();

            NpakAssetEntry ae{};
            ae.content_hash = entry.hash;
            ae.first_block = static_cast<uint32_t>(block_entries.Size());
            ae.uncompressed_size = static_cast<uint32_t>(remaining);

            // For the first block of this asset, compute offset_in_block.
            // Each asset starts at the beginning of its first block (offset 0).
            ae.offset_in_block = 0;

            // Split data into 64KB chunks and compress each
            while (remaining > 0)
            {
                size_t chunk_size = remaining < kBlockSize ? remaining : kBlockSize;
                wax::ByteSpan chunk{src, chunk_size};

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
                        size_t to_write = padding < sizeof(zeros) ? padding : sizeof(zeros);
                        std::fwrite(zeros, 1, to_write, file);
                        padding -= to_write;
                    }
                }

                uint64_t block_offset = static_cast<uint64_t>(std::ftell(file));

                // Try to compress
                auto compressed = Compress(chunk, entry.compression, *alloc_);

                NpakBlockEntry be{};
                be.file_offset = block_offset;

                if (compressed.Size() > 0)
                {
                    // Compression succeeded and was beneficial
                    be.compressed_size = static_cast<uint32_t>(compressed.Size());
                    be.compression = entry.compression;
                    std::fwrite(compressed.Data(), 1, compressed.Size(), file);
                }
                else
                {
                    // Store uncompressed
                    be.compressed_size = static_cast<uint32_t>(chunk_size);
                    be.compression = CompressionMethod::None;
                    std::fwrite(chunk.Data(), 1, chunk_size, file);
                }

                block_entries.PushBack(be);
                src += chunk_size;
                remaining -= chunk_size;
            }

            asset_entries.PushBack(ae);
        }

        // Sort asset entries by ContentHash for binary search
        // Simple insertion sort (sufficient for archive building)
        for (size_t i = 1; i < asset_entries.Size(); ++i)
        {
            NpakAssetEntry tmp = asset_entries[i];
            size_t j = i;
            while (j > 0 && tmp.content_hash < asset_entries[j - 1].content_hash)
            {
                asset_entries[j] = asset_entries[j - 1];
                --j;
            }
            asset_entries[j] = tmp;
        }

        // Write TOC
        uint64_t toc_offset = static_cast<uint64_t>(std::ftell(file));

        // TOC layout: [asset_count u32] [asset entries...] [block_count u32] [block entries...]
        uint32_t asset_count = static_cast<uint32_t>(asset_entries.Size());
        uint32_t block_count = static_cast<uint32_t>(block_entries.Size());

        // Build TOC buffer for CRC32
        size_t toc_size = sizeof(uint32_t) + asset_count * sizeof(NpakAssetEntry)
                        + sizeof(uint32_t) + block_count * sizeof(NpakBlockEntry);
        wax::ByteBuffer<> toc_buf{*alloc_};
        toc_buf.Resize(toc_size);
        uint8_t* toc_ptr = toc_buf.Data();

        std::memcpy(toc_ptr, &asset_count, sizeof(uint32_t));
        toc_ptr += sizeof(uint32_t);
        if (asset_count > 0)
        {
            std::memcpy(toc_ptr, &asset_entries[0], asset_count * sizeof(NpakAssetEntry));
            toc_ptr += asset_count * sizeof(NpakAssetEntry);
        }

        std::memcpy(toc_ptr, &block_count, sizeof(uint32_t));
        toc_ptr += sizeof(uint32_t);
        if (block_count > 0)
        {
            std::memcpy(toc_ptr, &block_entries[0], block_count * sizeof(NpakBlockEntry));
        }

        std::fwrite(toc_buf.Data(), 1, toc_size, file);

        // Finalize header
        header.block_count = block_count;
        header.toc_offset = toc_offset;
        header.toc_size = static_cast<uint32_t>(toc_size);
        header.toc_crc32 = Crc32(toc_buf.Data(), toc_size);

        std::fseek(file, 0, SEEK_SET);
        std::fwrite(&header, sizeof(NpakHeader), 1, file);

        std::fclose(file);
        return true;
    }
}
