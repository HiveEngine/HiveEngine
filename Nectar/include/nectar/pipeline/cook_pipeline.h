#pragma once

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <nectar/pipeline/i_asset_cooker.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <comb/default_allocator.h>

namespace nectar
{
    class CookerRegistry;
    class CasStore;
    class AssetDatabase;
    class CookCache;

    struct CookRequest
    {
        wax::Vector<AssetId> assets;
        wax::StringView platform;       // "pc", "ps5", etc.
        size_t worker_count{1};         // 1 = sequential
    };

    struct CookOutput
    {
        size_t total{0};
        size_t cooked{0};        // actually cooked
        size_t skipped{0};       // cache hit
        size_t failed{0};
        wax::Vector<AssetId> failed_assets;
    };

    /// Orchestrates the cook phase: intermediate → platform-optimized → CAS.
    class CookPipeline
    {
    public:
        CookPipeline(comb::DefaultAllocator& alloc,
                     CookerRegistry& registry,
                     CasStore& cas,
                     AssetDatabase& db,
                     CookCache& cache);

        /// Cook a batch of assets. Uses TopologicalSortLevels for parallel execution.
        [[nodiscard]] CookOutput CookAll(const CookRequest& request);

        /// Cook a single asset for a platform.
        [[nodiscard]] CookResult CookSingle(AssetId id, wax::StringView platform);

        /// Invalidate cook cache for all transitive dependents of `changed`.
        void InvalidateCascade(AssetId changed);

    private:
        void CookLevel(const wax::Vector<AssetId>& level,
                       wax::StringView platform,
                       size_t worker_count,
                       CookOutput& output);

        [[nodiscard]] ContentHash ComputeCookKey(AssetId id, wax::StringView platform);
        void CookAsset(AssetId id, wax::StringView platform, CookOutput& output);

        comb::DefaultAllocator* alloc_;
        CookerRegistry* registry_;
        CasStore* cas_;
        AssetDatabase* db_;
        CookCache* cache_;
    };
}
