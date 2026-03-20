#include <nectar/pipeline/cook_cache_persist.h>

#include <nectar/pipeline/cook_cache.h>

#include <wax/containers/string.h>

#include <cstdio>
#include <cstring>

namespace nectar
{
    namespace
    {
        constexpr uint32_t kMagic = 0x434B4F43; // "COKC"
        constexpr uint16_t kVersion = 1;

        struct EntryOnDisk
        {
            uint64_t m_compositeKey;
            uint64_t m_assetHigh;
            uint64_t m_assetLow;
            uint64_t m_cookKeyHigh;
            uint64_t m_cookKeyLow;
            uint64_t m_cookedHashHigh;
            uint64_t m_cookedHashLow;
            uint32_t m_cookerVersion;
            uint32_t m_pad;
        };
        static_assert(sizeof(EntryOnDisk) == 64);
    } // namespace

    bool SaveCookCache(wax::StringView path, const CookCache& cache, comb::DefaultAllocator& alloc)
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
        uint32_t entryCount = static_cast<uint32_t>(cache.Count());

        fwrite(&magic, 4, 1, f);
        fwrite(&version, 2, 1, f);
        fwrite(&reserved, 2, 1, f);
        fwrite(&entryCount, 4, 1, f);

        // Build a reverse map: composite key → AssetId
        wax::HashMap<uint64_t, AssetId> keyToAsset{alloc, entryCount};
        cache.ForEachAssetKeys([&](AssetId id, const wax::Vector<uint64_t>& keys) {
            for (size_t i = 0; i < keys.Size(); ++i)
                keyToAsset.Insert(keys[i], id);
        });

        cache.ForEachRaw([&](uint64_t compositeKey, const CookCacheEntry& entry) {
            EntryOnDisk d{};
            d.m_compositeKey = compositeKey;
            auto* assetId = keyToAsset.Find(compositeKey);
            if (assetId)
            {
                d.m_assetHigh = assetId->High();
                d.m_assetLow = assetId->Low();
            }
            d.m_cookKeyHigh = entry.m_cookKey.High();
            d.m_cookKeyLow = entry.m_cookKey.Low();
            d.m_cookedHashHigh = entry.m_cookedHash.High();
            d.m_cookedHashLow = entry.m_cookedHash.Low();
            d.m_cookerVersion = entry.m_cookerVersion;
            fwrite(&d, sizeof(d), 1, f);
        });

        fclose(f);
        return true;
    }

    bool LoadCookCache(wax::StringView path, CookCache& cache, comb::DefaultAllocator& alloc)
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
        uint32_t entryCount = 0;

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
        if (fread(&entryCount, 4, 1, f) != 1)
        {
            fclose(f);
            return false;
        }

        for (uint32_t i = 0; i < entryCount; ++i)
        {
            EntryOnDisk d{};
            if (fread(&d, sizeof(d), 1, f) != 1)
            {
                fclose(f);
                return false;
            }

            AssetId assetId{d.m_assetHigh, d.m_assetLow};
            CookCacheEntry entry{};
            entry.m_cookKey = ContentHash{d.m_cookKeyHigh, d.m_cookKeyLow};
            entry.m_cookedHash = ContentHash{d.m_cookedHashHigh, d.m_cookedHashLow};
            entry.m_cookerVersion = d.m_cookerVersion;
            cache.StoreRaw(d.m_compositeKey, assetId, entry);
        }

        fclose(f);
        return true;
    }
} // namespace nectar
