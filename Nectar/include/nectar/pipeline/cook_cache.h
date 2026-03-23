#pragma once

#include <hive/profiling/profiler.h>

#include <comb/default_allocator.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/span.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>

#include <mutex>

namespace nectar
{
    struct CookCacheEntry
    {
        ContentHash m_cookKey;
        ContentHash m_cookedHash; // hash of cooked blob in CAS
        uint32_t m_cookerVersion{0};
    };

    /// Thread-safe cache of cook results.
    /// Maps (AssetId, platform) → CookCacheEntry.
    class HIVE_API CookCache
    {
    public:
        explicit CookCache(comb::DefaultAllocator& alloc);

        /// Build the composite cook_key from all inputs that affect the cooked output.
        [[nodiscard]] static ContentHash BuildCookKey(ContentHash intermediateHash, uint32_t cookerVersion,
                                                      wax::StringView platform,
                                                      wax::Span<const ContentHash> depCookedHashes);

        /// Find cached entry for (asset, platform). nullptr if miss.
        [[nodiscard]] const CookCacheEntry* Find(AssetId id, wax::StringView platform) const;

        /// Store/update a cook result.
        void Store(AssetId id, wax::StringView platform, const CookCacheEntry& entry);

        /// Invalidate all platforms for an asset.
        void Invalidate(AssetId id);

        /// Invalidate a specific (asset, platform) entry.
        void Invalidate(AssetId id, wax::StringView platform);

        [[nodiscard]] size_t Count() const noexcept;

        /// Iterate all entries. F signature: void(AssetId id, wax::StringView platform, const CookCacheEntry&).
        /// Note: platform is not recoverable from the composite key, so the caller
        /// must provide an overload that accepts the raw (uint64_t key, CookCacheEntry).
        template <typename F>
        void ForEachRaw(F&& fn) const
        {
            std::lock_guard lock{m_mutex};
            for (auto it = m_entries.Begin(); it != m_entries.End(); ++it)
                fn(it.Key(), it.Value());
        }

        /// Iterate asset keys index. F signature: void(AssetId, const wax::Vector<uint64_t>&).
        template <typename F>
        void ForEachAssetKeys(F&& fn) const
        {
            std::lock_guard lock{m_mutex};
            for (auto it = m_assetKeys.Begin(); it != m_assetKeys.End(); ++it)
                fn(it.Key(), it.Value());
        }

        void StoreRaw(uint64_t compositeKey, AssetId id, const CookCacheEntry& entry);

    private:
        static uint64_t MakeKey(AssetId id, wax::StringView platform);

        comb::DefaultAllocator* m_alloc;
        wax::HashMap<uint64_t, CookCacheEntry> m_entries;
        wax::HashMap<AssetId, wax::Vector<uint64_t>> m_assetKeys; // secondary index for Invalidate(id)
        mutable HIVE_PROFILE_LOCKABLE_N(std::mutex, m_mutex, "Cook.CacheMutex");
    };
} // namespace nectar
