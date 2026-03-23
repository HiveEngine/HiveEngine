#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>

#include <nectar/pak/asset_manifest.h>
#include <nectar/pak/npak_format.h>

namespace nectar
{
    /// Builds a .npak archive from a set of blobs.
    class PakBuilder
    {
    public:
        HIVE_API explicit PakBuilder(comb::DefaultAllocator& alloc);

        HIVE_API void AddBlob(ContentHash hash, wax::ByteSpan data,
                              CompressionMethod compression = CompressionMethod::LZ4);

        HIVE_API void SetManifest(const AssetManifest& manifest);

        [[nodiscard]] HIVE_API bool Build(wax::StringView outputPath);

    private:
        struct BuildEntry
        {
            ContentHash m_hash;
            wax::ByteBuffer m_data;
            CompressionMethod m_compression;
        };

        comb::DefaultAllocator* m_alloc;
        wax::Vector<BuildEntry> m_entries;
        const AssetManifest* m_manifest{nullptr};
    };
} // namespace nectar
