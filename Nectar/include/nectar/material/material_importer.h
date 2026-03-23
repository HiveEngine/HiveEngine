#pragma once

#include <hive/hive_config.h>

#include <nectar/pipeline/asset_importer.h>

#include <nectar/material/material_data.h>

namespace nectar
{
    class HIVE_API MaterialImporter final : public AssetImporter<MaterialData>
    {
    public:
        [[nodiscard]] wax::Span<const char* const> SourceExtensions() const override;
        [[nodiscard]] uint32_t Version() const override;
        [[nodiscard]] wax::StringView TypeName() const override;

        ImportResult Import(wax::ByteSpan sourceData, const HiveDocument& settings, ImportContext& context) override;
    };
} // namespace nectar
