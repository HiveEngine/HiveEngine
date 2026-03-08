#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_buffer.h>

#include <nectar/pak/asset_manifest.h>
#include <nectar/pak/npak_format.h>

#include <cstdio>

namespace nectar
{
    /// Reads assets from a .npak archive by ContentHash.
    class PakReader
    {
    public:
        ~PakReader();

        // Non-copyable
        PakReader(const PakReader&) = delete;
        PakReader& operator=(const PakReader&) = delete;

        /// Open a .npak file. Returns nullptr on failure.
        [[nodiscard]] static PakReader* Open(wax::StringView path, comb::DefaultAllocator& alloc);

        /// Read an asset by ContentHash. Returns empty buffer if not found.
        [[nodiscard]] wax::ByteBuffer Read(ContentHash hash, comb::DefaultAllocator& alloc);

        /// Check if an asset exists.
        [[nodiscard]] bool Contains(ContentHash hash) const;

        /// Get the embedded asset manifest, or nullptr if none.
        [[nodiscard]] const AssetManifest* GetManifest() const;

        [[nodiscard]] size_t AssetCount() const noexcept;
        [[nodiscard]] size_t BlockCount() const noexcept;

        /// Get uncompressed size of an asset. Returns 0 if not found.
        [[nodiscard]] size_t GetAssetSize(ContentHash hash) const;

    private:
        PakReader() = default;

        /// Binary search for an asset entry. Returns nullptr if not found.
        [[nodiscard]] const NpakAssetEntry* FindAsset(ContentHash hash) const;

        comb::DefaultAllocator* m_alloc{nullptr};
        FILE* m_file{nullptr};
        NpakHeader m_header{};
        wax::Vector<NpakAssetEntry>* m_assetEntries{nullptr};
        wax::Vector<NpakBlockEntry>* m_blockEntries{nullptr};
        AssetManifest* m_manifest{nullptr};
    };
} // namespace nectar
