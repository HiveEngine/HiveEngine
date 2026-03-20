#include <nectar/material/material_importer.h>

#include <nectar/hive/hive_document.h>
#include <nectar/hive/hive_parser.h>
#include <nectar/pipeline/import_context.h>

#include <cstring>

namespace nectar
{
    namespace
    {
        uint8_t HexDigit(char c)
        {
            if (c >= '0' && c <= '9')
                return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f')
                return static_cast<uint8_t>(10 + c - 'a');
            if (c >= 'A' && c <= 'F')
                return static_cast<uint8_t>(10 + c - 'A');
            return 0;
        }

        AssetId ParseGuidString(wax::StringView str)
        {
            if (str.Size() < 34)
                return AssetId::Invalid();

            const char* s = str.Data();
            if (s[0] != '{' || s[33] != '}')
                return AssetId::Invalid();

            uint64_t high = 0;
            uint64_t low = 0;

            for (size_t i = 0; i < 16; ++i)
                high = (high << 4) | HexDigit(s[1 + i]);
            for (size_t i = 0; i < 16; ++i)
                low = (low << 4) | HexDigit(s[17 + i]);

            return AssetId{high, low};
        }

        AssetId ReadGuidField(const HiveDocument& doc, wax::StringView section, wax::StringView key)
        {
            auto val = doc.GetString(section, key);
            if (val.IsEmpty())
                return AssetId::Invalid();
            return ParseGuidString(val);
        }

        void DeclareTextureDep(ImportContext& ctx, AssetId textureId)
        {
            if (textureId.IsValid())
                ctx.DeclareHardDep(textureId);
        }
    } // namespace

    wax::Span<const char* const> MaterialImporter::SourceExtensions() const
    {
        static const char* const kExts[] = {".hmat"};
        return {kExts, 1};
    }

    uint32_t MaterialImporter::Version() const
    {
        return 1;
    }

    wax::StringView MaterialImporter::TypeName() const
    {
        return "Material";
    }

    ImportResult MaterialImporter::Import(wax::ByteSpan sourceData, const HiveDocument&, ImportContext& context)
    {
        ImportResult result;
        auto& alloc = context.GetAllocator();

        wax::StringView content{reinterpret_cast<const char*>(sourceData.Data()), sourceData.Size()};
        auto parseResult = HiveParser::Parse(content, alloc);
        if (!parseResult.Success())
        {
            result.m_errorMessage = wax::String{alloc, wax::StringView{"Failed to parse .hmat"}};
            return result;
        }

        const auto& doc = parseResult.m_document;

        DeclareTextureDep(context, ReadGuidField(doc, "textures", "albedo"));
        DeclareTextureDep(context, ReadGuidField(doc, "textures", "normal"));
        DeclareTextureDep(context, ReadGuidField(doc, "textures", "metallic_roughness"));
        DeclareTextureDep(context, ReadGuidField(doc, "textures", "emissive"));
        DeclareTextureDep(context, ReadGuidField(doc, "textures", "ao"));

        result.m_success = true;
        result.m_intermediateData = wax::ByteBuffer{alloc};
        result.m_intermediateData.Append(sourceData.Data(), sourceData.Size());

        return result;
    }
} // namespace nectar
