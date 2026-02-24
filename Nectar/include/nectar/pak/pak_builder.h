#pragma once

#include <nectar/pak/npak_format.h>
#include <nectar/pak/asset_manifest.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Builds a .npak archive from a set of blobs.
    class PakBuilder
    {
    public:
        explicit PakBuilder(comb::DefaultAllocator& alloc);

        /// Add a blob to be packed. Data is copied internally.
        void AddBlob(ContentHash hash, wax::ByteSpan data,
                     CompressionMethod compression = CompressionMethod::LZ4);

        /// Set the asset manifest to embed in the .npak.
        void SetManifest(const AssetManifest& manifest);

        /// Build the .npak and write to the given file path.
        /// Returns true on success.
        [[nodiscard]] bool Build(wax::StringView output_path);

    private:
        struct BuildEntry
        {
            ContentHash hash;
            wax::ByteBuffer<> data;
            CompressionMethod compression;
        };

        comb::DefaultAllocator* alloc_;
        wax::Vector<BuildEntry> entries_;
        const AssetManifest* manifest_{nullptr};
    };
}
