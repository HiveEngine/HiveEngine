#pragma once

#include <wax/containers/span.h>
#include <wax/containers/string_view.h>
#include <wax/serialization/byte_span.h>

#include <nectar/hive/hive_document.h>
#include <nectar/pipeline/import_context.h>
#include <nectar/pipeline/import_result.h>

namespace nectar
{
    /// Type-erased base for asset importers.
    /// The pipeline uses this interface — concrete importers inherit AssetImporter<T>
    /// which itself inherits IAssetImporter.
    class IAssetImporter
    {
    public:
        virtual ~IAssetImporter() = default;

        /// Source file extensions this importer handles (e.g. {".png", ".jpg"}).
        [[nodiscard]] virtual wax::Span<const char* const> SourceExtensions() const = 0;

        /// Importer version. Incrementing invalidates all previously imported assets.
        [[nodiscard]] virtual uint32_t Version() const = 0;

        /// Type name for the asset record (e.g. "Texture", "Mesh").
        [[nodiscard]] virtual wax::StringView TypeName() const = 0;

        /// Import source bytes + .hive settings into intermediate format.
        virtual ImportResult Import(wax::ByteSpan sourceData, const HiveDocument& settings, ImportContext& context) = 0;
    };
} // namespace nectar
