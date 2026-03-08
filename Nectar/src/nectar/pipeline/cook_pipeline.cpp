#include <hive/profiling/profiler.h>

#include <wax/containers/hash_set.h>

#include <nectar/cas/cas_store.h>
#include <nectar/database/asset_database.h>
#include <nectar/database/dependency_graph.h>
#include <nectar/pipeline/cook_cache.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/cooker_registry.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace nectar
{
    CookPipeline::CookPipeline(comb::DefaultAllocator& alloc, CookerRegistry& registry, CasStore& cas,
                               AssetDatabase& db, CookCache& cache)
        : m_alloc{&alloc}
        , m_registry{&registry}
        , m_cas{&cas}
        , m_db{&db}
        , m_cache{&cache} {}

    CookOutput CookPipeline::CookAll(const CookRequest& request) {
        HIVE_PROFILE_SCOPE_N("CookPipeline::CookAll");
        CookOutput output{};
        output.m_failedAssets = wax::Vector<AssetId>{*m_alloc};
        output.m_total = request.m_assets.Size();

        if (request.m_assets.IsEmpty())
            return output;

        // Build sub-graph for requested assets and sort by levels
        auto& graph = m_db->GetDependencyGraph();
        wax::Vector<wax::Vector<AssetId>> levels{*m_alloc};

        // Filter to only assets that exist in the graph
        // If no edges exist, each asset is its own level-0 node
        bool hasGraph = graph.TopologicalSortLevels(levels);
        if (!hasGraph)
        {
            // Cycle detected — cook all sequentially as fallback
            for (size_t i = 0; i < request.m_assets.Size(); ++i)
                CookAsset(request.m_assets[i], request.m_platform, output);
            return output;
        }

        // Build a set of requested asset IDs for filtering
        wax::HashSet<AssetId> requested{*m_alloc};
        for (size_t i = 0; i < request.m_assets.Size(); ++i)
            requested.Insert(request.m_assets[i]);

        // Cook level by level, filtering to only requested assets
        for (size_t lvl = 0; lvl < levels.Size(); ++lvl)
        {
            wax::Vector<AssetId> filtered{*m_alloc};
            for (size_t i = 0; i < levels[lvl].Size(); ++i)
            {
                if (requested.Contains(levels[lvl][i]))
                    filtered.PushBack(levels[lvl][i]);
            }

            if (!filtered.IsEmpty())
                CookLevel(filtered, request.m_platform, request.m_workerCount, output);
        }

        // Handle assets not in the graph (no dependencies registered)
        for (size_t i = 0; i < request.m_assets.Size(); ++i)
        {
            if (!graph.HasNode(request.m_assets[i]))
                CookAsset(request.m_assets[i], request.m_platform, output);
        }

        return output;
    }

    CookResult CookPipeline::CookSingle(AssetId id, wax::StringView platform) {
        HIVE_PROFILE_SCOPE_N("CookPipeline::CookSingle");
        CookResult result{};
        result.m_errorMessage = wax::String{*m_alloc};

        auto* record = m_db->FindByUuid(id);
        if (!record)
        {
            result.m_errorMessage.Append("Asset not found in database");
            return result;
        }

        IAssetCooker* cooker = m_registry->FindByType(record->m_type.View());
        if (!cooker)
        {
            result.m_errorMessage.Append("No cooker for type: ");
            result.m_errorMessage.Append(record->m_type.View().Data(), record->m_type.View().Size());
            return result;
        }

        if (!record->m_intermediateHash.IsValid())
        {
            result.m_errorMessage.Append("No intermediate data (asset not imported)");
            return result;
        }

        ContentHash cookKey = ComputeCookKey(id, platform);
        auto* cached = m_cache->Find(id, platform);
        if (cached && cached->m_cookKey == cookKey)
        {
            result.m_success = true;
            result.m_cookedData = m_cas->Load(cached->m_cookedHash);
            return result;
        }

        // Load intermediate from CAS
        auto intermediate = m_cas->Load(record->m_intermediateHash);
        if (intermediate.Size() == 0)
        {
            result.m_errorMessage.Append("Failed to load intermediate blob from CAS");
            return result;
        }

        // Cook
        CookContext ctx{platform, m_alloc};
        result = cooker->Cook(intermediate.View(), ctx);

        if (result.m_success)
        {
            ContentHash cookedHash = ContentHash::FromData(result.m_cookedData.View());
            (void)m_cas->Store(result.m_cookedData.View());

            CookCacheEntry entry{};
            entry.m_cookKey = cookKey;
            entry.m_cookedHash = cookedHash;
            entry.m_cookerVersion = cooker->Version();
            m_cache->Store(id, platform, entry);
        }

        return result;
    }

    void CookPipeline::InvalidateCascade(AssetId changed) {
        HIVE_PROFILE_SCOPE_N("CookPipeline::InvalidateCascade");
        auto& graph = m_db->GetDependencyGraph();
        wax::Vector<AssetId> dependents{*m_alloc};
        graph.GetTransitiveDependents(changed, DepKind::HARD | DepKind::BUILD, dependents);

        m_cache->Invalidate(changed);

        for (size_t i = 0; i < dependents.Size(); ++i)
            m_cache->Invalidate(dependents[i]);
    }

    void CookPipeline::CookLevel(const wax::Vector<AssetId>& level, wax::StringView platform, size_t workerCount,
                                 CookOutput& output) {
        HIVE_PROFILE_SCOPE_N("CookPipeline::CookLevel");
        if (workerCount <= 1 || level.Size() <= 1)
        {
            for (size_t i = 0; i < level.Size(); ++i)
                CookAsset(level[i], platform, output);
            return;
        }

        std::atomic<size_t> nextIndex{0};
        std::mutex outputMutex;

        // Each worker accumulates locally, then merges
        auto worker = [&]() {
            size_t localCooked = 0;
            size_t localSkipped = 0;
            size_t localFailed = 0;
            wax::Vector<AssetId> localFailedAssets{*m_alloc};

            while (true)
            {
                size_t idx = nextIndex.fetch_add(1);
                if (idx >= level.Size())
                    break;

                CookOutput localOut{};
                localOut.m_failedAssets = wax::Vector<AssetId>{*m_alloc};
                CookAsset(level[idx], platform, localOut);

                localCooked += localOut.m_cooked;
                localSkipped += localOut.m_skipped;
                localFailed += localOut.m_failed;
                for (size_t i = 0; i < localOut.m_failedAssets.Size(); ++i)
                    localFailedAssets.PushBack(localOut.m_failedAssets[i]);
            }

            std::lock_guard lock{outputMutex};
            output.m_cooked += localCooked;
            output.m_skipped += localSkipped;
            output.m_failed += localFailed;
            for (size_t i = 0; i < localFailedAssets.Size(); ++i)
                output.m_failedAssets.PushBack(localFailedAssets[i]);
        };

        size_t actual = workerCount < level.Size() ? workerCount : level.Size();
        std::vector<std::thread> threads;
        threads.reserve(actual);
        for (size_t i = 0; i < actual; ++i)
            threads.emplace_back(worker);
        for (auto& t : threads)
            t.join();
    }

    ContentHash CookPipeline::ComputeCookKey(AssetId id, wax::StringView platform) {
        auto* record = m_db->FindByUuid(id);
        if (!record)
            return ContentHash::Invalid();

        IAssetCooker* cooker = m_registry->FindByType(record->m_type.View());
        if (!cooker)
            return ContentHash::Invalid();

        // Gather cooked hashes of hard dependencies
        auto& graph = m_db->GetDependencyGraph();
        wax::Vector<AssetId> deps{*m_alloc};
        graph.GetDependencies(id, DepKind::HARD | DepKind::BUILD, deps);

        wax::Vector<ContentHash> depHashes{*m_alloc};
        for (size_t i = 0; i < deps.Size(); ++i)
        {
            auto* cached = m_cache->Find(deps[i], platform);
            if (cached)
                depHashes.PushBack(cached->m_cookedHash);
            else
                depHashes.PushBack(ContentHash::Invalid());
        }

        wax::Span<const ContentHash> depSpan{depHashes.Data(), depHashes.Size()};
        return CookCache::BuildCookKey(record->m_intermediateHash, cooker->Version(), platform, depSpan);
    }

    void CookPipeline::CookAsset(AssetId id, wax::StringView platform, CookOutput& output) {
        HIVE_PROFILE_SCOPE_N("CookPipeline::CookAsset");
        auto* record = m_db->FindByUuid(id);
        if (!record || !record->m_intermediateHash.IsValid())
        {
            ++output.m_failed;
            output.m_failedAssets.PushBack(id);
            return;
        }

        IAssetCooker* cooker = m_registry->FindByType(record->m_type.View());
        if (!cooker)
        {
            ++output.m_failed;
            output.m_failedAssets.PushBack(id);
            return;
        }

        ContentHash cookKey = ComputeCookKey(id, platform);
        auto* cached = m_cache->Find(id, platform);
        if (cached && cached->m_cookKey == cookKey)
        {
            ++output.m_skipped;
            return;
        }

        // Load intermediate
        auto intermediate = m_cas->Load(record->m_intermediateHash);
        if (intermediate.Size() == 0)
        {
            ++output.m_failed;
            output.m_failedAssets.PushBack(id);
            return;
        }

        // Cook
        CookContext ctx{platform, m_alloc};
        auto result = cooker->Cook(intermediate.View(), ctx);
        if (!result.m_success)
        {
            ++output.m_failed;
            output.m_failedAssets.PushBack(id);
            return;
        }

        ContentHash cookedHash = ContentHash::FromData(result.m_cookedData.View());
        (void)m_cas->Store(result.m_cookedData.View());

        CookCacheEntry entry{};
        entry.m_cookKey = cookKey;
        entry.m_cookedHash = cookedHash;
        entry.m_cookerVersion = cooker->Version();
        m_cache->Store(id, platform, entry);

        ++output.m_cooked;
    }
} // namespace nectar
