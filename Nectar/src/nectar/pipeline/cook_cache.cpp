#include <nectar/pipeline/cook_cache.h>
#include <cstring>

namespace nectar
{
    CookCache::CookCache(comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , entries_{alloc, 64}
        , asset_keys_{alloc, 64}
    {}

    ContentHash CookCache::BuildCookKey(
        ContentHash intermediate_hash,
        uint32_t cooker_version,
        wax::StringView platform,
        wax::Span<const ContentHash> dep_cooked_hashes)
    {
        constexpr size_t kHashBytes = 16;
        size_t buf_size = kHashBytes + sizeof(uint32_t) + platform.Size()
                          + dep_cooked_hashes.Size() * kHashBytes;

        uint8_t stack_buf[256];
        uint8_t* buf = stack_buf;
        bool heap = false;
        if (buf_size > sizeof(stack_buf))
        {
            buf = new uint8_t[buf_size];
            heap = true;
        }

        size_t offset = 0;

        uint64_t hi = intermediate_hash.High();
        uint64_t lo = intermediate_hash.Low();
        std::memcpy(buf + offset, &hi, 8); offset += 8;
        std::memcpy(buf + offset, &lo, 8); offset += 8;

        std::memcpy(buf + offset, &cooker_version, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        std::memcpy(buf + offset, platform.Data(), platform.Size());
        offset += platform.Size();

        for (size_t i = 0; i < dep_cooked_hashes.Size(); ++i)
        {
            uint64_t dhi = dep_cooked_hashes[i].High();
            uint64_t dlo = dep_cooked_hashes[i].Low();
            std::memcpy(buf + offset, &dhi, 8); offset += 8;
            std::memcpy(buf + offset, &dlo, 8); offset += 8;
        }

        auto result = ContentHash::FromData(buf, offset);
        if (heap) delete[] buf;
        return result;
    }

    const CookCacheEntry* CookCache::Find(AssetId id, wax::StringView platform) const
    {
        std::lock_guard lock{mutex_};
        uint64_t key = MakeKey(id, platform);
        return entries_.Find(key);
    }

    void CookCache::Store(AssetId id, wax::StringView platform, const CookCacheEntry& entry)
    {
        std::lock_guard lock{mutex_};
        uint64_t key = MakeKey(id, platform);

        auto* existing = entries_.Find(key);
        if (existing)
        {
            *existing = entry;
        }
        else
        {
            entries_.Insert(key, entry);

            // Track key in secondary index
            auto* keys = asset_keys_.Find(id);
            if (keys)
            {
                keys->PushBack(key);
            }
            else
            {
                wax::Vector<uint64_t> v{*alloc_};
                v.PushBack(key);
                asset_keys_.Insert(id, static_cast<wax::Vector<uint64_t>&&>(v));
            }
        }
    }

    void CookCache::Invalidate(AssetId id)
    {
        std::lock_guard lock{mutex_};
        auto* keys = asset_keys_.Find(id);
        if (!keys) return;

        for (size_t i = 0; i < keys->Size(); ++i)
            entries_.Remove((*keys)[i]);

        asset_keys_.Remove(id);
    }

    void CookCache::Invalidate(AssetId id, wax::StringView platform)
    {
        std::lock_guard lock{mutex_};
        uint64_t key = MakeKey(id, platform);
        entries_.Remove(key);

        // Remove from secondary index
        auto* keys = asset_keys_.Find(id);
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
                asset_keys_.Remove(id);
        }
    }

    size_t CookCache::Count() const noexcept
    {
        std::lock_guard lock{mutex_};
        return entries_.Count();
    }

    uint64_t CookCache::MakeKey(AssetId id, wax::StringView platform)
    {
        uint64_t h = id.Hash();
        constexpr uint64_t kFnvPrime = 0x00000100000001B3;
        for (size_t i = 0; i < platform.Size(); ++i)
        {
            h ^= static_cast<uint64_t>(platform[i]);
            h *= kFnvPrime;
        }
        return h;
    }
}
