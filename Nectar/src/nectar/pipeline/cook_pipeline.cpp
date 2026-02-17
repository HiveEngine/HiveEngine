#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/cooker_registry.h>
#include <nectar/pipeline/cook_cache.h>
#include <nectar/cas/cas_store.h>
#include <nectar/database/asset_database.h>
#include <nectar/database/dependency_graph.h>
#include <hive/profiling/profiler.h>
#include <wax/containers/hash_set.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

namespace nectar
{
    CookPipeline::CookPipeline(comb::DefaultAllocator& alloc,
                               CookerRegistry& registry,
                               CasStore& cas,
                               AssetDatabase& db,
                               CookCache& cache)
        : alloc_{&alloc}
        , registry_{&registry}
        , cas_{&cas}
        , db_{&db}
        , cache_{&cache}
    {}

    CookOutput CookPipeline::CookAll(const CookRequest& request)
    {
        HIVE_PROFILE_SCOPE_N("CookPipeline::CookAll");
        CookOutput output{};
        output.failed_assets = wax::Vector<AssetId>{*alloc_};
        output.total = request.assets.Size();

        if (request.assets.IsEmpty())
            return output;

        // Build sub-graph for requested assets and sort by levels
        auto& graph = db_->GetDependencyGraph();
        wax::Vector<wax::Vector<AssetId>> levels{*alloc_};

        // Filter to only assets that exist in the graph
        // If no edges exist, each asset is its own level-0 node
        bool has_graph = graph.TopologicalSortLevels(levels);
        if (!has_graph)
        {
            // Cycle detected — cook all sequentially as fallback
            for (size_t i = 0; i < request.assets.Size(); ++i)
                CookAsset(request.assets[i], request.platform, output);
            return output;
        }

        // Build a set of requested asset IDs for filtering
        wax::HashSet<AssetId> requested{*alloc_};
        for (size_t i = 0; i < request.assets.Size(); ++i)
            requested.Insert(request.assets[i]);

        // Cook level by level, filtering to only requested assets
        for (size_t lvl = 0; lvl < levels.Size(); ++lvl)
        {
            wax::Vector<AssetId> filtered{*alloc_};
            for (size_t i = 0; i < levels[lvl].Size(); ++i)
            {
                if (requested.Contains(levels[lvl][i]))
                    filtered.PushBack(levels[lvl][i]);
            }

            if (!filtered.IsEmpty())
                CookLevel(filtered, request.platform, request.worker_count, output);
        }

        // Handle assets not in the graph (no dependencies registered)
        for (size_t i = 0; i < request.assets.Size(); ++i)
        {
            if (!graph.HasNode(request.assets[i]))
                CookAsset(request.assets[i], request.platform, output);
        }

        return output;
    }

    CookResult CookPipeline::CookSingle(AssetId id, wax::StringView platform)
    {
        HIVE_PROFILE_SCOPE_N("CookPipeline::CookSingle");
        CookResult result{};
        result.error_message = wax::String<>{*alloc_};

        auto* record = db_->FindByUuid(id);
        if (!record)
        {
            result.error_message.Append("Asset not found in database");
            return result;
        }

        IAssetCooker* cooker = registry_->FindByType(record->type.View());
        if (!cooker)
        {
            result.error_message.Append("No cooker for type: ");
            result.error_message.Append(record->type.View().Data(), record->type.View().Size());
            return result;
        }

        if (!record->intermediate_hash.IsValid())
        {
            result.error_message.Append("No intermediate data (asset not imported)");
            return result;
        }

        // Check cache
        ContentHash cook_key = ComputeCookKey(id, platform);
        auto* cached = cache_->Find(id, platform);
        if (cached && cached->cook_key == cook_key)
        {
            // Cache hit — load cooked data from CAS
            result.success = true;
            result.cooked_data = cas_->Load(cached->cooked_hash);
            return result;
        }

        // Load intermediate from CAS
        auto intermediate = cas_->Load(record->intermediate_hash);
        if (intermediate.Size() == 0)
        {
            result.error_message.Append("Failed to load intermediate blob from CAS");
            return result;
        }

        // Cook
        CookContext ctx{platform, alloc_};
        result = cooker->Cook(intermediate.View(), ctx);

        if (result.success)
        {
            // Store cooked data in CAS
            ContentHash cooked_hash = ContentHash::FromData(result.cooked_data.View());
            (void)cas_->Store(result.cooked_data.View());

            // Update cache
            CookCacheEntry entry{};
            entry.cook_key = cook_key;
            entry.cooked_hash = cooked_hash;
            entry.cooker_version = cooker->Version();
            cache_->Store(id, platform, entry);
        }

        return result;
    }

    void CookPipeline::InvalidateCascade(AssetId changed)
    {
        HIVE_PROFILE_SCOPE_N("CookPipeline::InvalidateCascade");
        auto& graph = db_->GetDependencyGraph();
        wax::Vector<AssetId> dependents{*alloc_};
        graph.GetTransitiveDependents(changed, DepKind::Hard | DepKind::Build, dependents);

        // Invalidate the changed asset itself
        cache_->Invalidate(changed);

        // Invalidate all transitive dependents
        for (size_t i = 0; i < dependents.Size(); ++i)
            cache_->Invalidate(dependents[i]);
    }

    void CookPipeline::CookLevel(const wax::Vector<AssetId>& level,
                                  wax::StringView platform,
                                  size_t worker_count,
                                  CookOutput& output)
    {
        HIVE_PROFILE_SCOPE_N("CookPipeline::CookLevel");
        if (worker_count <= 1 || level.Size() <= 1)
        {
            for (size_t i = 0; i < level.Size(); ++i)
                CookAsset(level[i], platform, output);
            return;
        }

        std::atomic<size_t> next_index{0};
        std::mutex output_mutex;

        // Each worker accumulates locally, then merges
        auto worker = [&]() {
            size_t local_cooked = 0;
            size_t local_skipped = 0;
            size_t local_failed = 0;
            wax::Vector<AssetId> local_failed_assets{*alloc_};

            while (true)
            {
                size_t idx = next_index.fetch_add(1);
                if (idx >= level.Size()) break;

                CookOutput local_out{};
                local_out.failed_assets = wax::Vector<AssetId>{*alloc_};
                CookAsset(level[idx], platform, local_out);

                local_cooked += local_out.cooked;
                local_skipped += local_out.skipped;
                local_failed += local_out.failed;
                for (size_t i = 0; i < local_out.failed_assets.Size(); ++i)
                    local_failed_assets.PushBack(local_out.failed_assets[i]);
            }

            std::lock_guard lock{output_mutex};
            output.cooked += local_cooked;
            output.skipped += local_skipped;
            output.failed += local_failed;
            for (size_t i = 0; i < local_failed_assets.Size(); ++i)
                output.failed_assets.PushBack(local_failed_assets[i]);
        };

        size_t actual = worker_count < level.Size() ? worker_count : level.Size();
        std::vector<std::thread> threads;
        threads.reserve(actual);
        for (size_t i = 0; i < actual; ++i)
            threads.emplace_back(worker);
        for (auto& t : threads)
            t.join();
    }

    ContentHash CookPipeline::ComputeCookKey(AssetId id, wax::StringView platform)
    {
        auto* record = db_->FindByUuid(id);
        if (!record) return ContentHash::Invalid();

        IAssetCooker* cooker = registry_->FindByType(record->type.View());
        if (!cooker) return ContentHash::Invalid();

        // Gather cooked hashes of hard dependencies
        auto& graph = db_->GetDependencyGraph();
        wax::Vector<AssetId> deps{*alloc_};
        graph.GetDependencies(id, DepKind::Hard | DepKind::Build, deps);

        wax::Vector<ContentHash> dep_hashes{*alloc_};
        for (size_t i = 0; i < deps.Size(); ++i)
        {
            auto* cached = cache_->Find(deps[i], platform);
            if (cached)
                dep_hashes.PushBack(cached->cooked_hash);
            else
                dep_hashes.PushBack(ContentHash::Invalid());
        }

        wax::Span<const ContentHash> dep_span{dep_hashes.Data(), dep_hashes.Size()};
        return CookCache::BuildCookKey(
            record->intermediate_hash,
            cooker->Version(),
            platform,
            dep_span);
    }

    void CookPipeline::CookAsset(AssetId id, wax::StringView platform, CookOutput& output)
    {
        HIVE_PROFILE_SCOPE_N("CookPipeline::CookAsset");
        auto* record = db_->FindByUuid(id);
        if (!record || !record->intermediate_hash.IsValid())
        {
            ++output.failed;
            output.failed_assets.PushBack(id);
            return;
        }

        IAssetCooker* cooker = registry_->FindByType(record->type.View());
        if (!cooker)
        {
            ++output.failed;
            output.failed_assets.PushBack(id);
            return;
        }

        // Check cache
        ContentHash cook_key = ComputeCookKey(id, platform);
        auto* cached = cache_->Find(id, platform);
        if (cached && cached->cook_key == cook_key)
        {
            ++output.skipped;
            return;
        }

        // Load intermediate
        auto intermediate = cas_->Load(record->intermediate_hash);
        if (intermediate.Size() == 0)
        {
            ++output.failed;
            output.failed_assets.PushBack(id);
            return;
        }

        // Cook
        CookContext ctx{platform, alloc_};
        auto result = cooker->Cook(intermediate.View(), ctx);
        if (!result.success)
        {
            ++output.failed;
            output.failed_assets.PushBack(id);
            return;
        }

        // Store in CAS
        ContentHash cooked_hash = ContentHash::FromData(result.cooked_data.View());
        (void)cas_->Store(result.cooked_data.View());

        // Update cache
        CookCacheEntry entry{};
        entry.cook_key = cook_key;
        entry.cooked_hash = cooked_hash;
        entry.cooker_version = cooker->Version();
        cache_->Store(id, platform, entry);

        ++output.cooked;
    }
}
