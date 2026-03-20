#include <nectar/registry/hiveid_file.h>

#include <wax/containers/string_view.h>

#include <cstdio>
#include <cstring>

namespace
{
    constexpr char kHex[] = "0123456789abcdef";

    void AssetIdToHex(nectar::AssetId id, char out[33])
    {
        uint64_t high = id.High();
        uint64_t low = id.Low();

        for (size_t i = 0; i < 8; ++i)
        {
            uint8_t byte = static_cast<uint8_t>(high >> (56 - i * 8));
            out[i * 2] = kHex[byte >> 4];
            out[i * 2 + 1] = kHex[byte & 0x0F];
        }
        for (size_t i = 0; i < 8; ++i)
        {
            uint8_t byte = static_cast<uint8_t>(low >> (56 - i * 8));
            out[16 + i * 2] = kHex[byte >> 4];
            out[16 + i * 2 + 1] = kHex[byte & 0x0F];
        }
        out[32] = '\0';
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

    nectar::AssetId HexToAssetId(const char* hex, size_t len)
    {
        if (len != 32)
            return nectar::AssetId::Invalid();

        uint64_t high = 0;
        uint64_t low = 0;

        for (size_t i = 0; i < 16; ++i)
            high = (high << 4) | HexDigit(hex[i]);
        for (size_t i = 0; i < 16; ++i)
            low = (low << 4) | HexDigit(hex[16 + i]);

        return nectar::AssetId{high, low};
    }
    bool ContainsPathTraversal(const char* path)
    {
        for (const char* p = path; *p; ++p)
        {
            if (p[0] == '.' && p[1] == '.')
            {
                if (p == path || p[-1] == '/' || p[-1] == '\\')
                {
                    char after = p[2];
                    if (after == '\0' || after == '/' || after == '\\')
                        return true;
                }
            }
        }
        return false;
    }
} // namespace

namespace nectar
{
    bool WriteHiveId(const char* assetPath, const HiveIdData& data)
    {
        if (ContainsPathTraversal(assetPath))
            return false;

        char pathBuf[1024];
        std::snprintf(pathBuf, sizeof(pathBuf), "%s.hiveid", assetPath);

        FILE* f = nullptr;
#ifdef _MSC_VER
        fopen_s(&f, pathBuf, "wb");
#else
        f = fopen(pathBuf, "wb");
#endif
        if (!f)
            return false;

        char hexBuf[33];
        AssetIdToHex(data.m_guid, hexBuf);

        std::fprintf(f, "guid=%s\ntype=%s\n", hexBuf, data.m_type.CStr());
        std::fclose(f);
        return true;
    }

    bool ReadHiveId(const char* assetPath, HiveIdData& data, comb::DefaultAllocator& alloc)
    {
        if (ContainsPathTraversal(assetPath))
            return false;

        char pathBuf[1024];
        std::snprintf(pathBuf, sizeof(pathBuf), "%s.hiveid", assetPath);

        FILE* f = nullptr;
#ifdef _MSC_VER
        fopen_s(&f, pathBuf, "rb");
#else
        f = fopen(pathBuf, "rb");
#endif
        if (!f)
            return false;

        char line[256];
        data.m_guid = AssetId::Invalid();
        data.m_type = wax::String{alloc};

        while (std::fgets(line, sizeof(line), f))
        {
            size_t len = std::strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                line[--len] = '\0';

            if (std::strncmp(line, "guid=", 5) == 0)
            {
                data.m_guid = HexToAssetId(line + 5, len - 5);
            }
            else if (std::strncmp(line, "type=", 5) == 0)
            {
                data.m_type = wax::String{alloc, wax::StringView{line + 5, len - 5}};
            }
        }

        std::fclose(f);
        return data.m_guid.IsValid();
    }
} // namespace nectar
