#pragma once

#include <nectar/pipeline/asset_importer.h>
#include <nectar/texture/texture_data.h>

namespace nectar
{
    /// Imports image files (.png, .jpg, .bmp, .tga, .hdr) into NTEX intermediate format.
    /// Reads settings from HiveDocument [import] section:
    ///   max_size (int, 0=no limit), srgb (bool, true),
    ///   generate_mipmaps (bool, true), flip_y (bool, false)
    class TextureImporter final : public AssetImporter<NtexHeader>
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
