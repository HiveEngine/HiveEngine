#pragma once

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>
#include <wax/containers/string_view.h>
#include <wax/containers/span.h>
#include <comb/default_allocator.h>
#include <hive/profiling/profiler.h>
#include <mutex>

namespace nectar
{
    struct CookCacheEntry
    {
        ContentHash cook_key;
        ContentHash cooked_hash;     // hash of cooked blob in CAS
        uint32_t cooker_version{0};
    };

    /// Thread-safe cache of cook results.
    /// Maps (AssetId, platform) → CookCacheEntry.
    class CookCache
    {
    public:
        explicit CookCache(comb::DefaultAllocator& alloc);

        /// Build the composite cook_key from all inputs that affect the cooked output.
        [[nodiscard]] static ContentHash BuildCookKey(
            ContentHash intermediate_hash,
            uint32_t cooker_version,
            wax::StringView platform,
            wax::Span<const ContentHash> dep_cooked_hashes);

        /// Find cached entry for (asset, platform). nullptr if miss.
        [[nodiscard]] const CookCacheEntry* Find(AssetId id, wax::StringView platform) const;

        /// Store/update a cook result.
        void Store(AssetId id, wax::StringView platform, const CookCacheEntry& entry);

        /// Invalidate all platforms for an asset.
        void Invalidate(AssetId id);

        /// Invalidate a specific (asset, platform) entry.
        void Invalidate(AssetId id, wax::StringView platform);

        [[nodiscard]] size_t Count() const noexcept;

    private:
        static uint64_t MakeKey(AssetId id, wax::StringView platform);

        comb::DefaultAllocator* alloc_;
        wax::HashMap<uint64_t, CookCacheEntry> entries_;
        wax::HashMap<AssetId, wax::Vector<uint64_t>> asset_keys_;  // secondary index for Invalidate(id)
        mutable HIVE_PROFILE_LOCKABLE_N(std::mutex, mutex_, "Cook.CacheMutex");
    };
}
