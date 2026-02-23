#define _CRT_SECURE_NO_WARNINGS
#include <nectar/pak/pak_reader.h>
#include <nectar/pak/compression.h>
#include <nectar/pak/crc32.h>
#include <hive/profiling/profiler.h>
#include <cstring>

namespace nectar
{
    PakReader::~PakReader()
    {
        if (file_)
            std::fclose(file_);
        if (manifest_ && alloc_)
        {
            manifest_->~AssetManifest();
            alloc_->Deallocate(manifest_);
        }
        if (asset_entries_ && alloc_)
        {
            asset_entries_->~Vector();
            alloc_->Deallocate(asset_entries_);
        }
        if (block_entries_ && alloc_)
        {
            block_entries_->~Vector();
            alloc_->Deallocate(block_entries_);
        }
    }

    PakReader* PakReader::Open(wax::StringView path, comb::DefaultAllocator& alloc)
    {
        HIVE_PROFILE_SCOPE_N("PakReader::Open");
        wax::String<> path_str{alloc};
        path_str.Append(path.Data(), path.Size());

        FILE* file = std::fopen(path_str.CStr(), "rb");
        if (!file) return nullptr;

        // Read header
        NpakHeader header{};
        if (std::fread(&header, sizeof(NpakHeader), 1, file) != 1)
        {
            std::fclose(file);
            return nullptr;
        }

        if (header.magic != kNpakMagic || header.version != kNpakVersion)
        {
            std::fclose(file);
            return nullptr;
        }

        // Read TOC
        if (std::fseek(file, static_cast<long>(header.toc_offset), SEEK_SET) != 0)
        {
            std::fclose(file);
            return nullptr;
        }

        wax::ByteBuffer<> toc_buf{alloc};
        toc_buf.Resize(header.toc_size);
        if (std::fread(toc_buf.Data(), 1, header.toc_size, file) != header.toc_size)
        {
            std::fclose(file);
            return nullptr;
        }

        uint32_t computed_crc = Crc32(toc_buf.Data(), header.toc_size);
        if (computed_crc != header.toc_crc32)
        {
            std::fclose(file);
            return nullptr;
        }

        // Parse TOC
        const uint8_t* ptr = toc_buf.Data();
        const uint8_t* end = ptr + header.toc_size;

        if (ptr + sizeof(uint32_t) > end) { std::fclose(file); return nullptr; }
        uint32_t asset_count = 0;
        std::memcpy(&asset_count, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        size_t asset_bytes = asset_count * sizeof(NpakAssetEntry);
        if (ptr + asset_bytes > end) { std::fclose(file); return nullptr; }

        // Allocate vectors on heap via allocator
        void* av_mem = alloc.Allocate(sizeof(wax::Vector<NpakAssetEntry>), alignof(wax::Vector<NpakAssetEntry>));
        auto* asset_vec = new (av_mem) wax::Vector<NpakAssetEntry>{alloc};
        asset_vec->Resize(asset_count);
        if (asset_count > 0)
            std::memcpy(&(*asset_vec)[0], ptr, asset_bytes);
        ptr += asset_bytes;

        if (ptr + sizeof(uint32_t) > end)
        {
            asset_vec->~Vector();
            alloc.Deallocate(av_mem);
            std::fclose(file);
            return nullptr;
        }
        uint32_t block_count = 0;
        std::memcpy(&block_count, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        size_t block_bytes = block_count * sizeof(NpakBlockEntry);
        if (ptr + block_bytes > end)
        {
            asset_vec->~Vector();
            alloc.Deallocate(av_mem);
            std::fclose(file);
            return nullptr;
        }

        void* bv_mem = alloc.Allocate(sizeof(wax::Vector<NpakBlockEntry>), alignof(wax::Vector<NpakBlockEntry>));
        auto* block_vec = new (bv_mem) wax::Vector<NpakBlockEntry>{alloc};
        block_vec->Resize(block_count);
        if (block_count > 0)
            std::memcpy(&(*block_vec)[0], ptr, block_bytes);

        // Build reader
        void* reader_mem = alloc.Allocate(sizeof(PakReader), alignof(PakReader));
        auto* reader = new (reader_mem) PakReader{};
        reader->alloc_ = &alloc;
        reader->file_ = file;
        reader->header_ = header;
        reader->asset_entries_ = asset_vec;
        reader->block_entries_ = block_vec;

        // Try to load manifest
        const NpakAssetEntry* manifest_entry = reader->FindAsset(kManifestSentinel);
        if (manifest_entry)
        {
            auto manifest_blob = reader->Read(kManifestSentinel, alloc);
            if (manifest_blob.Size() > 0)
            {
                void* m_mem = alloc.Allocate(sizeof(AssetManifest), alignof(AssetManifest));
                reader->manifest_ = new (m_mem) AssetManifest{
                    AssetManifest::Deserialize(manifest_blob.View(), alloc)};
            }
        }

        return reader;
    }

    wax::ByteBuffer<> PakReader::Read(ContentHash hash, comb::DefaultAllocator& alloc)
    {
        HIVE_PROFILE_SCOPE_N("PakReader::Read");
        wax::ByteBuffer<> result{alloc};

        const NpakAssetEntry* entry = FindAsset(hash);
        if (!entry) return result;

        size_t remaining = entry->uncompressed_size;
        result.Resize(remaining);

        // Calculate how many blocks this asset spans
        size_t offset_in_first_block = entry->offset_in_block;
        size_t dst_offset = 0;
        uint32_t block_idx = entry->first_block;

        while (remaining > 0 && block_idx < block_entries_->Size())
        {
            const auto& be = (*block_entries_)[block_idx];

            // Read compressed block from file
            wax::ByteBuffer<> compressed{alloc};
            compressed.Resize(be.compressed_size);

            std::fseek(file_, static_cast<long>(be.file_offset), SEEK_SET);
            size_t read = std::fread(compressed.Data(), 1, be.compressed_size, file_);
            if (read != be.compressed_size)
            {
                result.Clear();
                return result;
            }

            // Decompress block
            size_t block_uncompressed = kBlockSize;
            // Last block might be smaller
            if (remaining + offset_in_first_block < kBlockSize)
                block_uncompressed = remaining + offset_in_first_block;

            wax::ByteBuffer<> decompressed{alloc};
            if (be.compression == CompressionMethod::None)
            {
                decompressed = static_cast<wax::ByteBuffer<>&&>(compressed);
            }
            else
            {
                decompressed = Decompress(
                    compressed.View(), block_uncompressed, be.compression, alloc);
                if (decompressed.Size() == 0)
                {
                    result.Clear();
                    return result;
                }
            }

            // Copy relevant portion to result
            size_t copy_offset = (block_idx == entry->first_block) ? offset_in_first_block : 0;
            size_t available = decompressed.Size() - copy_offset;
            size_t to_copy = remaining < available ? remaining : available;

            std::memcpy(result.Data() + dst_offset, decompressed.Data() + copy_offset, to_copy);
            dst_offset += to_copy;
            remaining -= to_copy;
            ++block_idx;
        }

        if (remaining > 0)
        {
            result.Clear();
            return result;
        }

        return result;
    }

    bool PakReader::Contains(ContentHash hash) const
    {
        return FindAsset(hash) != nullptr;
    }

    const AssetManifest* PakReader::GetManifest() const
    {
        return manifest_;
    }

    size_t PakReader::AssetCount() const noexcept
    {
        return asset_entries_ ? asset_entries_->Size() : 0;
    }

    size_t PakReader::BlockCount() const noexcept
    {
        return block_entries_ ? block_entries_->Size() : 0;
    }

    size_t PakReader::GetAssetSize(ContentHash hash) const
    {
        const auto* entry = FindAsset(hash);
        return entry ? entry->uncompressed_size : 0;
    }

    const NpakAssetEntry* PakReader::FindAsset(ContentHash hash) const
    {
        if (!asset_entries_ || asset_entries_->Size() == 0)
            return nullptr;

        // Binary search — entries are sorted by content_hash
        size_t lo = 0;
        size_t hi = asset_entries_->Size();
        while (lo < hi)
        {
            size_t mid = lo + (hi - lo) / 2;
            const auto& mid_hash = (*asset_entries_)[mid].content_hash;
            if (mid_hash == hash)
                return &(*asset_entries_)[mid];
            if (mid_hash < hash)
                lo = mid + 1;
            else
                hi = mid;
        }
        return nullptr;
    }
}
