#include <nectar/material/material_serializer.h>

#include <nectar/hive/hive_document.h>
#include <nectar/hive/hive_parser.h>
#include <nectar/hive/hive_writer.h>

#include <cstdio>
#include <cstring>

namespace nectar
{
    namespace
    {
        constexpr char kHex[] = "0123456789abcdef";

        wax::StringView BlendModeToString(MaterialData::BlendMode mode)
        {
            switch (mode)
            {
                case MaterialData::BlendMode::ALPHA_TEST:
                    return "alpha_test";
                case MaterialData::BlendMode::ALPHA_BLEND:
                    return "alpha_blend";
                default:
                    return "opaque";
            }
        }

        MaterialData::BlendMode StringToBlendMode(wax::StringView str)
        {
            if (str == wax::StringView{"alpha_test"})
                return MaterialData::BlendMode::ALPHA_TEST;
            if (str == wax::StringView{"alpha_blend"})
                return MaterialData::BlendMode::ALPHA_BLEND;
            return MaterialData::BlendMode::BLEND_OPAQUE;
        }

        wax::String FormatBaseColor(const float (&f)[4], comb::DefaultAllocator& alloc)
        {
            char buf[128];
            int len = std::snprintf(buf, sizeof(buf), "%g %g %g %g", f[0], f[1], f[2], f[3]);
            return wax::String{alloc, wax::StringView{buf, static_cast<size_t>(len)}};
        }

        void ParseBaseColor(wax::StringView str, float (&out)[4])
        {
            char buf[128];
            size_t len = str.Size() < sizeof(buf) - 1 ? str.Size() : sizeof(buf) - 1;
            std::memcpy(buf, str.Data(), len);
            buf[len] = '\0';

            float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
            std::sscanf(buf, "%f %f %f %f", &r, &g, &b, &a);
            out[0] = r;
            out[1] = g;
            out[2] = b;
            out[3] = a;
        }

        wax::String AssetIdToGuidString(AssetId id, comb::DefaultAllocator& alloc)
        {
            char buf[35];
            uint64_t high = id.High();
            uint64_t low = id.Low();

            buf[0] = '{';
            for (size_t i = 0; i < 8; ++i)
            {
                uint8_t byte = static_cast<uint8_t>(high >> (56 - i * 8));
                buf[1 + i * 2] = kHex[byte >> 4];
                buf[1 + i * 2 + 1] = kHex[byte & 0x0F];
            }
            for (size_t i = 0; i < 8; ++i)
            {
                uint8_t byte = static_cast<uint8_t>(low >> (56 - i * 8));
                buf[17 + i * 2] = kHex[byte >> 4];
                buf[17 + i * 2 + 1] = kHex[byte & 0x0F];
            }
            buf[33] = '}';
            buf[34] = '\0';

            return wax::String{alloc, wax::StringView{buf, 34}};
        }

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

        void WriteGuidField(HiveDocument& doc, wax::StringView section, wax::StringView key, AssetId id,
                            comb::DefaultAllocator& alloc)
        {
            if (id.IsValid())
                doc.SetValue(section, key, HiveValue::MakeString(alloc, AssetIdToGuidString(id, alloc).View()));
        }

        AssetId ReadGuidField(const HiveDocument& doc, wax::StringView section, wax::StringView key)
        {
            auto val = doc.GetString(section, key);
            if (val.IsEmpty())
                return AssetId::Invalid();
            return ParseGuidString(val);
        }
    } // namespace

    bool SaveMaterial(const MaterialData& mat, wax::StringView path, comb::DefaultAllocator& alloc)
    {
        HiveDocument doc{alloc};

        doc.SetValue("material", "name", HiveValue::MakeString(alloc, mat.m_name.View()));
        doc.SetValue("material", "shader", HiveValue::MakeString(alloc, mat.m_shader.View()));
        doc.SetValue("material", "blend", HiveValue::MakeString(alloc, BlendModeToString(mat.m_blendMode)));
        doc.SetValue("material", "double_sided", HiveValue::MakeBool(mat.m_doubleSided));
        doc.SetValue("material", "alpha_cutoff", HiveValue::MakeFloat(static_cast<double>(mat.m_alphaCutoff)));

        WriteGuidField(doc, "textures", "albedo", mat.m_albedoTexture, alloc);
        WriteGuidField(doc, "textures", "normal", mat.m_normalTexture, alloc);
        WriteGuidField(doc, "textures", "metallic_roughness", mat.m_metallicRoughnessTexture, alloc);
        WriteGuidField(doc, "textures", "emissive", mat.m_emissiveTexture, alloc);
        WriteGuidField(doc, "textures", "ao", mat.m_aoTexture, alloc);

        doc.SetValue("factors", "base_color",
                     HiveValue::MakeString(alloc, FormatBaseColor(mat.m_baseColorFactor, alloc).View()));
        doc.SetValue("factors", "metallic", HiveValue::MakeFloat(static_cast<double>(mat.m_metallicFactor)));
        doc.SetValue("factors", "roughness", HiveValue::MakeFloat(static_cast<double>(mat.m_roughnessFactor)));

        wax::String content = HiveWriter::Write(doc, alloc);
        wax::String filePath{alloc, path};

        FILE* file = std::fopen(filePath.CStr(), "wb");
        if (!file)
            return false;

        if (content.Size() > 0)
            std::fwrite(content.CStr(), 1, content.Size(), file);

        std::fclose(file);
        return true;
    }

    bool LoadMaterial(MaterialData& mat, wax::StringView path, comb::DefaultAllocator& alloc)
    {
        wax::String filePath{alloc, path};
        FILE* file = std::fopen(filePath.CStr(), "rb");
        if (!file)
            return false;

        std::fseek(file, 0, SEEK_END);
        long fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        if (fileSize <= 0)
        {
            std::fclose(file);
            return false;
        }

        wax::String content{alloc};
        content.Reserve(static_cast<size_t>(fileSize));

        char buffer[4096];
        size_t remaining = static_cast<size_t>(fileSize);
        while (remaining > 0)
        {
            size_t toRead = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            size_t bytesRead = std::fread(buffer, 1, toRead, file);
            if (bytesRead == 0)
                break;
            content.Append(buffer, bytesRead);
            remaining -= bytesRead;
        }
        std::fclose(file);

        auto parseResult = HiveParser::Parse(content.View(), alloc);
        if (!parseResult.Success())
            return false;

        const auto& doc = parseResult.m_document;

        mat.m_name = wax::String{alloc, doc.GetString("material", "name")};
        mat.m_shader = wax::String{alloc, doc.GetString("material", "shader", "standard_pbr")};
        mat.m_blendMode = StringToBlendMode(doc.GetString("material", "blend", "opaque"));
        mat.m_doubleSided = doc.GetBool("material", "double_sided", false);
        mat.m_alphaCutoff = static_cast<float>(doc.GetFloat("material", "alpha_cutoff", 0.5));

        mat.m_albedoTexture = ReadGuidField(doc, "textures", "albedo");
        mat.m_normalTexture = ReadGuidField(doc, "textures", "normal");
        mat.m_metallicRoughnessTexture = ReadGuidField(doc, "textures", "metallic_roughness");
        mat.m_emissiveTexture = ReadGuidField(doc, "textures", "emissive");
        mat.m_aoTexture = ReadGuidField(doc, "textures", "ao");

        wax::StringView baseColorStr = doc.GetString("factors", "base_color");
        if (!baseColorStr.IsEmpty())
        {
            ParseBaseColor(baseColorStr, mat.m_baseColorFactor);
        }
        else
        {
            mat.m_baseColorFactor[0] = 1.f;
            mat.m_baseColorFactor[1] = 1.f;
            mat.m_baseColorFactor[2] = 1.f;
            mat.m_baseColorFactor[3] = 1.f;
        }

        mat.m_metallicFactor = static_cast<float>(doc.GetFloat("factors", "metallic", 1.0));
        mat.m_roughnessFactor = static_cast<float>(doc.GetFloat("factors", "roughness", 1.0));

        return true;
    }
} // namespace nectar
