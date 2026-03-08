#include <hive/profiling/profiler.h>

#include <nectar/pak/compression.h>
#include <nectar/pak/crc32.h>
#include <nectar/pak/pak_reader.h>

#include <cstring>
#include <memory>

namespace nectar
{
    PakReader::~PakReader() {
        if (m_file)
            std::fclose(m_file);
        if (m_manifest && m_alloc)
        {
            std::destroy_at(m_manifest);
            m_alloc->Deallocate(m_manifest);
        }
        if (m_assetEntries && m_alloc)
        {
            std::destroy_at(m_assetEntries);
            m_alloc->Deallocate(m_assetEntries);
        }
        if (m_blockEntries && m_alloc)
        {
            std::destroy_at(m_blockEntries);
            m_alloc->Deallocate(m_blockEntries);
        }
    }

    PakReader* PakReader::Open(wax::StringView path, comb::DefaultAllocator& alloc) {
        HIVE_PROFILE_SCOPE_N("PakReader::Open");
        wax::String pathStr{alloc};
        pathStr.Append(path.Data(), path.Size());

        FILE* file = std::fopen(pathStr.CStr(), "rb");
        if (!file)
            return nullptr;

        // Read header
        NpakHeader header{};
        if (std::fread(&header, sizeof(NpakHeader), 1, file) != 1)
        {
            std::fclose(file);
            return nullptr;
        }

        if (header.m_magic != kNpakMagic || header.m_version != kNpakVersion)
        {
            std::fclose(file);
            return nullptr;
        }

        // Read TOC
        if (std::fseek(file, static_cast<long>(header.m_tocOffset), SEEK_SET) != 0)
        {
            std::fclose(file);
            return nullptr;
        }

        wax::ByteBuffer tocBuf{alloc};
        tocBuf.Resize(header.m_tocSize);
        if (std::fread(tocBuf.Data(), 1, header.m_tocSize, file) != header.m_tocSize)
        {
            std::fclose(file);
            return nullptr;
        }

        uint32_t computedCrc = Crc32(tocBuf.Data(), header.m_tocSize);
        if (computedCrc != header.m_tocCrc32)
        {
            std::fclose(file);
            return nullptr;
        }

        // Parse TOC
        const uint8_t* ptr = tocBuf.Data();
        const uint8_t* end = ptr + header.m_tocSize;

        if (ptr + sizeof(uint32_t) > end)
        {
            std::fclose(file);
            return nullptr;
        }
        uint32_t assetCount = 0;
        std::memcpy(&assetCount, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        size_t assetBytes = assetCount * sizeof(NpakAssetEntry);
        if (ptr + assetBytes > end)
        {
            std::fclose(file);
            return nullptr;
        }

        // Allocate vectors on heap via allocator
        void* avMem = alloc.Allocate(sizeof(wax::Vector<NpakAssetEntry>), alignof(wax::Vector<NpakAssetEntry>));
        auto* assetVec = new (avMem) wax::Vector<NpakAssetEntry>{alloc};
        assetVec->Resize(assetCount);
        if (assetCount > 0)
            std::memcpy(&(*assetVec)[0], ptr, assetBytes);
        ptr += assetBytes;

        if (ptr + sizeof(uint32_t) > end)
        {
            std::destroy_at(assetVec);
            alloc.Deallocate(avMem);
            std::fclose(file);
            return nullptr;
        }
        uint32_t blockCount = 0;
        std::memcpy(&blockCount, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        size_t blockBytes = blockCount * sizeof(NpakBlockEntry);
        if (ptr + blockBytes > end)
        {
            std::destroy_at(assetVec);
            alloc.Deallocate(avMem);
            std::fclose(file);
            return nullptr;
        }

        void* bvMem = alloc.Allocate(sizeof(wax::Vector<NpakBlockEntry>), alignof(wax::Vector<NpakBlockEntry>));
        auto* blockVec = new (bvMem) wax::Vector<NpakBlockEntry>{alloc};
        blockVec->Resize(blockCount);
        if (blockCount > 0)
            std::memcpy(&(*blockVec)[0], ptr, blockBytes);

        // Build reader
        void* readerMem = alloc.Allocate(sizeof(PakReader), alignof(PakReader));
        auto* reader = new (readerMem) PakReader{};
        reader->m_alloc = &alloc;
        reader->m_file = file;
        reader->m_header = header;
        reader->m_assetEntries = assetVec;
        reader->m_blockEntries = blockVec;

        // Try to load manifest
        const NpakAssetEntry* manifestEntry = reader->FindAsset(kManifestSentinel);
        if (manifestEntry)
        {
            auto manifestBlob = reader->Read(kManifestSentinel, alloc);
            if (manifestBlob.Size() > 0)
            {
                void* mMem = alloc.Allocate(sizeof(AssetManifest), alignof(AssetManifest));
                reader->m_manifest = new (mMem) AssetManifest{AssetManifest::Deserialize(manifestBlob.View(), alloc)};
            }
        }

        return reader;
    }

    wax::ByteBuffer PakReader::Read(ContentHash hash, comb::DefaultAllocator& alloc) {
        HIVE_PROFILE_SCOPE_N("PakReader::Read");
        wax::ByteBuffer result{alloc};

        const NpakAssetEntry* entry = FindAsset(hash);
        if (!entry)
            return result;

        size_t remaining = entry->m_uncompressedSize;
        result.Resize(remaining);

        // Calculate how many blocks this asset spans
        size_t offsetInFirstBlock = entry->m_offsetInBlock;
        size_t dstOffset = 0;
        uint32_t blockIdx = entry->m_firstBlock;

        while (remaining > 0 && blockIdx < m_blockEntries->Size())
        {
            const auto& be = (*m_blockEntries)[blockIdx];

            // Read compressed block from file
            wax::ByteBuffer compressed{alloc};
            compressed.Resize(be.m_compressedSize);

            std::fseek(m_file, static_cast<long>(be.m_fileOffset), SEEK_SET);
            size_t read = std::fread(compressed.Data(), 1, be.m_compressedSize, m_file);
            if (read != be.m_compressedSize)
            {
                result.Clear();
                return result;
            }

            // Decompress block
            size_t blockUncompressed = kBlockSize;
            // Last block might be smaller
            if (remaining + offsetInFirstBlock < kBlockSize)
                blockUncompressed = remaining + offsetInFirstBlock;

            wax::ByteBuffer decompressed{alloc};
            if (be.m_compression == CompressionMethod::NONE)
            {
                decompressed = static_cast<wax::ByteBuffer&&>(compressed);
            }
            else
            {
                decompressed = Decompress(compressed.View(), blockUncompressed, be.m_compression, alloc);
                if (decompressed.Size() == 0)
                {
                    result.Clear();
                    return result;
                }
            }

            // Copy relevant portion to result
            size_t copyOffset = (blockIdx == entry->m_firstBlock) ? offsetInFirstBlock : 0;
            size_t available = decompressed.Size() - copyOffset;
            size_t toCopy = remaining < available ? remaining : available;

            std::memcpy(result.Data() + dstOffset, decompressed.Data() + copyOffset, toCopy);
            dstOffset += toCopy;
            remaining -= toCopy;
            ++blockIdx;
        }

        if (remaining > 0)
        {
            result.Clear();
            return result;
        }

        return result;
    }

    bool PakReader::Contains(ContentHash hash) const {
        return FindAsset(hash) != nullptr;
    }

    const AssetManifest* PakReader::GetManifest() const {
        return m_manifest;
    }

    size_t PakReader::AssetCount() const noexcept {
        return m_assetEntries ? m_assetEntries->Size() : 0;
    }

    size_t PakReader::BlockCount() const noexcept {
        return m_blockEntries ? m_blockEntries->Size() : 0;
    }

    size_t PakReader::GetAssetSize(ContentHash hash) const {
        const auto* entry = FindAsset(hash);
        return entry ? entry->m_uncompressedSize : 0;
    }

    const NpakAssetEntry* PakReader::FindAsset(ContentHash hash) const {
        if (!m_assetEntries || m_assetEntries->Size() == 0)
            return nullptr;

        // Binary search — entries are sorted by content_hash
        size_t lo = 0;
        size_t hi = m_assetEntries->Size();
        while (lo < hi)
        {
            size_t mid = lo + (hi - lo) / 2;
            const auto& midHash = (*m_assetEntries)[mid].m_contentHash;
            if (midHash == hash)
                return &(*m_assetEntries)[mid];
            if (midHash < hash)
                lo = mid + 1;
            else
                hi = mid;
        }
        return nullptr;
    }
} // namespace nectar
