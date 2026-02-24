#pragma once

#include <nectar/pipeline/asset_importer.h>
#include <nectar/mesh/mesh_data.h>

namespace nectar
{
    /// Imports glTF 2.0 mesh files (.gltf/.glb) into NMSH intermediate format.
    /// Settings from HiveDocument [import] section:
    ///   scale (float, 1.0), flip_uv (bool, false), generate_normals (bool, true),
    ///   base_path (string, "") — filesystem path to .gltf file for resolving external .bin
    class GltfImporter final : public AssetImporter<NmshHeader>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override;
        uint32_t Version() const override;
        wax::StringView TypeName() const override;

        ImportResult Import(wax::ByteSpan source_data,
                            const HiveDocument& settings,
                            ImportContext& context) override;
    };
}
