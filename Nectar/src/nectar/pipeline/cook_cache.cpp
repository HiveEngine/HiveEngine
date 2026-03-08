#include <nectar/pipeline/cook_cache.h>

#include <cstring>

namespace nectar
{
    CookCache::CookCache(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_entries{alloc, 64}
        , m_assetKeys{alloc, 64} {}

    ContentHash CookCache::BuildCookKey(ContentHash intermediateHash, uint32_t cookerVersion, wax::StringView platform,
                                        wax::Span<const ContentHash> depCookedHashes) {
        constexpr size_t kHashBytes = 16;
        size_t bufSize = kHashBytes + sizeof(uint32_t) + platform.Size() + depCookedHashes.Size() * kHashBytes;

        uint8_t stackBuf[256];
        uint8_t* buf = stackBuf;
        bool heap = false;
        if (bufSize > sizeof(stackBuf))
        {
            buf = new uint8_t[bufSize];
            heap = true;
        }

        size_t offset = 0;

        uint64_t hi = intermediateHash.High();
        uint64_t lo = intermediateHash.Low();
        std::memcpy(buf + offset, &hi, 8);
        offset += 8;
        std::memcpy(buf + offset, &lo, 8);
        offset += 8;

        std::memcpy(buf + offset, &cookerVersion, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        std::memcpy(buf + offset, platform.Data(), platform.Size());
        offset += platform.Size();

        for (size_t i = 0; i < depCookedHashes.Size(); ++i)
        {
            uint64_t dhi = depCookedHashes[i].High();
            uint64_t dlo = depCookedHashes[i].Low();
            std::memcpy(buf + offset, &dhi, 8);
            offset += 8;
            std::memcpy(buf + offset, &dlo, 8);
            offset += 8;
        }

        auto result = ContentHash::FromData(buf, offset);
        if (heap)
            delete[] buf;
        return result;
    }

    const CookCacheEntry* CookCache::Find(AssetId id, wax::StringView platform) const {
        std::lock_guard lock{m_mutex};
        uint64_t key = MakeKey(id, platform);
        return m_entries.Find(key);
    }

    void CookCache::Store(AssetId id, wax::StringView platform, const CookCacheEntry& entry) {
        std::lock_guard lock{m_mutex};
        uint64_t key = MakeKey(id, platform);

        auto* existing = m_entries.Find(key);
        if (existing)
        {
            *existing = entry;
        }
        else
        {
            m_entries.Insert(key, entry);

            // Track key in secondary index
            auto* keys = m_assetKeys.Find(id);
            if (keys)
            {
                keys->PushBack(key);
            }
            else
            {
                wax::Vector<uint64_t> v{*m_alloc};
                v.PushBack(key);
                m_assetKeys.Insert(id, static_cast<wax::Vector<uint64_t>&&>(v));
            }
        }
    }

    void CookCache::Invalidate(AssetId id) {
        std::lock_guard lock{m_mutex};
        auto* keys = m_assetKeys.Find(id);
        if (!keys)
            return;

        for (size_t i = 0; i < keys->Size(); ++i)
            m_entries.Remove((*keys)[i]);

        m_assetKeys.Remove(id);
    }

    void CookCache::Invalidate(AssetId id, wax::StringView platform) {
        std::lock_guard lock{m_mutex};
        uint64_t key = MakeKey(id, platform);
        m_entries.Remove(key);

        // Remove from secondary index
        auto* keys = m_assetKeys.Find(id);
        if (keys)
        {
            for (size_t i = 0; i < keys->Size(); ++i)
            {
                if ((*keys)[i] == key)
                {
                    // Swap-and-pop
                    if (i < keys->Size() - 1)
                        (*keys)[i] = (*keys)[keys->Size() - 1];
                    keys->PopBack();
                    break;
                }
            }
            if (keys->IsEmpty())
                m_assetKeys.Remove(id);
        }
    }

    size_t CookCache::Count() const noexcept {
        std::lock_guard lock{m_mutex};
        return m_entries.Count();
    }

    uint64_t CookCache::MakeKey(AssetId id, wax::StringView platform) {
        uint64_t h = id.Hash();
        constexpr uint64_t kFnvPrime = 0x00000100000001B3;
        for (size_t i = 0; i < platform.Size(); ++i)
        {
            h ^= static_cast<uint64_t>(platform[i]);
            h *= kFnvPrime;
        }
        return h;
    }
} // namespace nectar
