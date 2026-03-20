#include <nectar/database/dep_cache.h>

#include <nectar/database/dependency_graph.h>

#include <wax/containers/string.h>

#include <cstdio>
#include <cstring>

namespace nectar
{
    namespace
    {
        constexpr uint32_t kMagic = 0x47504544; // "DEPG"
        constexpr uint16_t kVersion = 1;

        struct EdgeOnDisk
        {
            uint64_t m_fromHigh;
            uint64_t m_fromLow;
            uint64_t m_toHigh;
            uint64_t m_toLow;
            uint8_t m_kind;
            uint8_t m_pad[7];
        };
        static_assert(sizeof(EdgeOnDisk) == 40);
    } // namespace

    bool SaveDependencyCache(wax::StringView path, const DependencyGraph& graph, comb::DefaultAllocator& alloc)
    {
        wax::String filePath{alloc, path};
        FILE* f = nullptr;
#ifdef _MSC_VER
        fopen_s(&f, filePath.CStr(), "wb");
#else
        f = fopen(filePath.CStr(), "wb");
#endif
        if (!f)
            return false;

        uint32_t magic = kMagic;
        uint16_t version = kVersion;
        uint16_t reserved = 0;
        uint32_t edgeCount = static_cast<uint32_t>(graph.EdgeCount());

        fwrite(&magic, 4, 1, f);
        fwrite(&version, 2, 1, f);
        fwrite(&reserved, 2, 1, f);
        fwrite(&edgeCount, 4, 1, f);

        graph.ForEachEdge([&](const DependencyEdge& edge) {
            EdgeOnDisk d{};
            d.m_fromHigh = edge.m_from.High();
            d.m_fromLow = edge.m_from.Low();
            d.m_toHigh = edge.m_to.High();
            d.m_toLow = edge.m_to.Low();
            d.m_kind = static_cast<uint8_t>(edge.m_kind);
            fwrite(&d, sizeof(d), 1, f);
        });

        fclose(f);
        return true;
    }

    bool LoadDependencyCache(wax::StringView path, DependencyGraph& graph, comb::DefaultAllocator& alloc)
    {
        wax::String filePath{alloc, path};
        FILE* f = nullptr;
#ifdef _MSC_VER
        fopen_s(&f, filePath.CStr(), "rb");
#else
        f = fopen(filePath.CStr(), "rb");
#endif
        if (!f)
            return false;

        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t reserved = 0;
        uint32_t edgeCount = 0;

        if (fread(&magic, 4, 1, f) != 1 || magic != kMagic)
        {
            fclose(f);
            return false;
        }
        if (fread(&version, 2, 1, f) != 1 || version != kVersion)
        {
            fclose(f);
            return false;
        }
        fread(&reserved, 2, 1, f);
        if (fread(&edgeCount, 4, 1, f) != 1)
        {
            fclose(f);
            return false;
        }

        for (uint32_t i = 0; i < edgeCount; ++i)
        {
            EdgeOnDisk d{};
            if (fread(&d, sizeof(d), 1, f) != 1)
            {
                fclose(f);
                return false;
            }

            AssetId from{d.m_fromHigh, d.m_fromLow};
            AssetId to{d.m_toHigh, d.m_toLow};
            auto kind = static_cast<DepKind>(d.m_kind);
            graph.AddEdge(from, to, kind);
        }

        fclose(f);
        return true;
    }
} // namespace nectar
