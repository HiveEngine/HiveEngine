#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <nectar/pipeline/i_asset_cooker.h>

#include <drone/job_submitter.h>

namespace nectar
{
    class CookerRegistry;
    class CasStore;
    class AssetDatabase;
    class CookCache;

    struct CookRequest
    {
        wax::Vector<AssetId> m_assets;
        wax::StringView m_platform; // "pc", "ps5", etc.
        size_t m_workerCount{1};    // 1 = sequential
    };

    struct CookOutput
    {
        size_t m_total{0};
        size_t m_cooked{0};  // actually cooked
        size_t m_skipped{0}; // cache hit
        size_t m_failed{0};
        wax::Vector<AssetId> m_failedAssets;
    };

    /// Orchestrates the cook phase: intermediate → platform-optimized → CAS.
    class CookPipeline
    {
    public:
        CookPipeline(comb::DefaultAllocator& alloc, CookerRegistry& registry, CasStore& cas, AssetDatabase& db,
                     CookCache& cache, drone::JobSubmitter jobs = {});

        /// Cook a batch of assets. Uses TopologicalSortLevels for parallel execution.
        [[nodiscard]] CookOutput CookAll(const CookRequest& request);

        /// Cook a single asset for a platform.
        [[nodiscard]] CookResult CookSingle(AssetId id, wax::StringView platform);

        /// Invalidate cook cache for all transitive dependents of `changed`.
        void InvalidateCascade(AssetId changed);

    private:
        void CookLevel(const wax::Vector<AssetId>& level, wax::StringView platform, size_t workerCount,
                       CookOutput& output);

        [[nodiscard]] ContentHash ComputeCookKey(AssetId id, wax::StringView platform);
        void CookAsset(AssetId id, wax::StringView platform, CookOutput& output);

        comb::DefaultAllocator* m_alloc;
        drone::JobSubmitter m_jobs;
        CookerRegistry* m_registry;
        CasStore* m_cas;
        AssetDatabase* m_db;
        CookCache* m_cache;
    };
} // namespace nectar
