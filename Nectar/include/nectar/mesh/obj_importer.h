#pragma once

#include <nectar/pipeline/asset_importer.h>
#include <nectar/mesh/mesh_data.h>

namespace nectar
{
    /// Imports OBJ mesh files into NMSH intermediate format.
    /// Reads settings from HiveDocument [import] section:
    ///   scale (float, 1.0), flip_uv (bool, false), generate_normals (bool, true)
    class ObjImporter final : public AssetImporter<NmshHeader>
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
