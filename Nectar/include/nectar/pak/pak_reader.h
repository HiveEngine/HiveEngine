#pragma once

#include <nectar/pak/npak_format.h>
#include <nectar/pak/asset_manifest.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>
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
        [[nodiscard]] static PakReader* Open(
            wax::StringView path, comb::DefaultAllocator& alloc);

        /// Read an asset by ContentHash. Returns empty buffer if not found.
        [[nodiscard]] wax::ByteBuffer<> Read(ContentHash hash, comb::DefaultAllocator& alloc);

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

        comb::DefaultAllocator* alloc_{nullptr};
        FILE* file_{nullptr};
        NpakHeader header_{};
        wax::Vector<NpakAssetEntry>* asset_entries_{nullptr};
        wax::Vector<NpakBlockEntry>* block_entries_{nullptr};
        AssetManifest* manifest_{nullptr};
    };
}
