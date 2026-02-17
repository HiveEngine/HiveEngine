#include <larvae/larvae.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/cooker_registry.h>
#include <nectar/pipeline/cook_cache.h>
#include <nectar/pipeline/asset_cooker.h>
#include <nectar/cas/cas_store.h>
#include <nectar/database/asset_database.h>
#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <filesystem>

namespace {

    auto& GetCookAlloc()
    {
        static comb::ModuleAllocator alloc{"TestCookPipe", 8 * 1024 * 1024};
        return alloc.Get();
    }

    struct TempDir
    {
        std::filesystem::path path;
        explicit TempDir(const char* name)
        {
            path = std::filesystem::temp_directory_path() / name;
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            std::filesystem::create_directories(path);
        }
        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
        wax::StringView View() const
        {
            static thread_local std::string s;
            s = path.string();
            return wax::StringView{s.c_str()};
        }
    };

    nectar::AssetId MakeId(uint64_t v)
    {
        uint8_t bytes[16] = {};
        std::memcpy(bytes, &v, sizeof(v));
        return nectar::AssetId::FromBytes(bytes);
    }

    // Simple pass-through cooker: copies intermediate as-is
    struct DummyCooked {};

    class PassthroughCooker final : public nectar::AssetCooker<DummyCooked>
    {
    public:
        wax::StringView TypeName() const override { return "TestType"; }
        uint32_t Version() const override { return version_; }
        nectar::CookResult Cook(wax::ByteSpan data, const nectar::CookContext& ctx) override
        {
            nectar::CookResult r;
            r.success = true;
            r.cooked_data = wax::ByteBuffer<>{*ctx.alloc};
            r.cooked_data.Append(data.Data(), data.Size());
            return r;
        }
        uint32_t version_{1};
    };

    class FailCooker final : public nectar::AssetCooker<DummyCooked>
    {
    public:
        wax::StringView TypeName() const override { return "FailType"; }
        uint32_t Version() const override { return 1; }
        nectar::CookResult Cook(wax::ByteSpan, const nectar::CookContext& ctx) override
        {
            nectar::CookResult r;
            r.error_message = wax::String<>{*ctx.alloc, "cook failed"};
            return r;
        }
    };

    // Helper: insert an asset record and store intermediate in CAS
    nectar::ContentHash SetupAsset(nectar::AssetDatabase& db, nectar::CasStore& cas,
                                    nectar::AssetId id, const char* path, const char* type,
                                    const void* data, size_t size)
    {
        auto& alloc = GetCookAlloc();
        wax::ByteSpan span{data, size};
        nectar::ContentHash cas_hash = cas.Store(span);

        nectar::AssetRecord record{};
        record.uuid = id;
        record.path = wax::String<>{alloc};
        record.path.Append(path, std::strlen(path));
        record.type = wax::String<>{alloc};
        record.type.Append(type, std::strlen(type));
        record.name = wax::String<>{alloc};
        record.content_hash = nectar::ContentHash::FromData(data, size);
        record.intermediate_hash = cas_hash;
        record.import_version = 1;
        record.labels = wax::Vector<wax::String<>>{alloc};
        db.Insert(static_cast<nectar::AssetRecord&&>(record));

        return cas_hash;
    }

    // =========================================================================
    // CookSingle
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarCookPipe", "CookSingleSuccess", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_1"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        auto id = MakeId(1);
        const char* data = "intermediate blob";
        SetupAsset(db, cas, id, "test.dat", "TestType", data, std::strlen(data));

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};
        auto result = pipe.CookSingle(id, "pc");
        larvae::AssertTrue(result.success);
        larvae::AssertEqual(result.cooked_data.Size(), std::strlen(data));
    });

    auto t2 = larvae::RegisterTest("NectarCookPipe", "CookSingleNoCooker", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_2"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};

        auto id = MakeId(1);
        const char* data = "x";
        SetupAsset(db, cas, id, "test.dat", "UnknownType", data, 1);

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};
        auto result = pipe.CookSingle(id, "pc");
        larvae::AssertFalse(result.success);
    });

    auto t3 = larvae::RegisterTest("NectarCookPipe", "CookSingleNoRecord", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_3"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};
        auto result = pipe.CookSingle(MakeId(99), "pc");
        larvae::AssertFalse(result.success);
    });

    auto t4 = larvae::RegisterTest("NectarCookPipe", "CookSingleNoIntermediate", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_4"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        // Insert record with invalid intermediate_hash
        auto id = MakeId(1);
        nectar::AssetRecord record{};
        record.uuid = id;
        record.path = wax::String<>{alloc, "test.dat"};
        record.type = wax::String<>{alloc, "TestType"};
        record.name = wax::String<>{alloc};
        record.labels = wax::Vector<wax::String<>>{alloc};
        db.Insert(static_cast<nectar::AssetRecord&&>(record));

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};
        auto result = pipe.CookSingle(id, "pc");
        larvae::AssertFalse(result.success);
    });

    auto t5 = larvae::RegisterTest("NectarCookPipe", "CookSingleCacheHit", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_5"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        auto id = MakeId(1);
        const char* data = "cached blob";
        SetupAsset(db, cas, id, "test.dat", "TestType", data, std::strlen(data));

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};

        // First cook
        auto r1 = pipe.CookSingle(id, "pc");
        larvae::AssertTrue(r1.success);
        larvae::AssertEqual(cache.Count(), size_t{1});

        // Second cook — should hit cache
        auto r2 = pipe.CookSingle(id, "pc");
        larvae::AssertTrue(r2.success);
        larvae::AssertEqual(r2.cooked_data.Size(), std::strlen(data));
    });

    // =========================================================================
    // CookAll
    // =========================================================================

    auto t6 = larvae::RegisterTest("NectarCookPipe", "CookAllSequential", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_6"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        auto a = MakeId(1);
        auto b = MakeId(2);
        SetupAsset(db, cas, a, "a.dat", "TestType", "aaa", 3);
        SetupAsset(db, cas, b, "b.dat", "TestType", "bbb", 3);

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};

        nectar::CookRequest req{};
        req.assets = wax::Vector<nectar::AssetId>{alloc};
        req.assets.PushBack(a);
        req.assets.PushBack(b);
        req.platform = "pc";
        req.worker_count = 1;

        auto out = pipe.CookAll(req);
        larvae::AssertEqual(out.total, size_t{2});
        larvae::AssertEqual(out.cooked, size_t{2});
        larvae::AssertEqual(out.failed, size_t{0});
        larvae::AssertEqual(out.skipped, size_t{0});
    });

    auto t7 = larvae::RegisterTest("NectarCookPipe", "CookAllParallel", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_7"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        SetupAsset(db, cas, a, "a.dat", "TestType", "aaa", 3);
        SetupAsset(db, cas, b, "b.dat", "TestType", "bbb", 3);
        SetupAsset(db, cas, c, "c.dat", "TestType", "ccc", 3);

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};

        nectar::CookRequest req{};
        req.assets = wax::Vector<nectar::AssetId>{alloc};
        req.assets.PushBack(a);
        req.assets.PushBack(b);
        req.assets.PushBack(c);
        req.platform = "pc";
        req.worker_count = 2;

        auto out = pipe.CookAll(req);
        larvae::AssertEqual(out.total, size_t{3});
        larvae::AssertEqual(out.cooked, size_t{3});
        larvae::AssertEqual(out.failed, size_t{0});
    });

    auto t8 = larvae::RegisterTest("NectarCookPipe", "CookAllWithDeps", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_8"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        auto a = MakeId(1);
        auto b = MakeId(2);
        SetupAsset(db, cas, a, "a.dat", "TestType", "aaa", 3);
        SetupAsset(db, cas, b, "b.dat", "TestType", "bbb", 3);

        // a depends on b (a -> b means a needs b)
        db.GetDependencyGraph().AddEdge(a, b, nectar::DepKind::Hard);

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};

        nectar::CookRequest req{};
        req.assets = wax::Vector<nectar::AssetId>{alloc};
        req.assets.PushBack(a);
        req.assets.PushBack(b);
        req.platform = "pc";
        req.worker_count = 1;

        auto out = pipe.CookAll(req);
        larvae::AssertEqual(out.total, size_t{2});
        larvae::AssertEqual(out.cooked, size_t{2});
        larvae::AssertEqual(out.failed, size_t{0});

        // Both should be cached
        larvae::AssertEqual(cache.Count(), size_t{2});
    });

    auto t9 = larvae::RegisterTest("NectarCookPipe", "CookAllCacheSkip", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_9"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        auto a = MakeId(1);
        SetupAsset(db, cas, a, "a.dat", "TestType", "data", 4);

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};

        nectar::CookRequest req{};
        req.assets = wax::Vector<nectar::AssetId>{alloc};
        req.assets.PushBack(a);
        req.platform = "pc";
        req.worker_count = 1;

        // First cook
        auto out1 = pipe.CookAll(req);
        larvae::AssertEqual(out1.cooked, size_t{1});

        // Second cook — same data → cache hit
        auto out2 = pipe.CookAll(req);
        larvae::AssertEqual(out2.skipped, size_t{1});
        larvae::AssertEqual(out2.cooked, size_t{0});
    });

    // =========================================================================
    // Invalidation
    // =========================================================================

    auto t10 = larvae::RegisterTest("NectarCookPipe", "InvalidateCascade", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_10"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        auto a = MakeId(1);
        auto b = MakeId(2);
        auto c = MakeId(3);
        SetupAsset(db, cas, a, "a.dat", "TestType", "aaa", 3);
        SetupAsset(db, cas, b, "b.dat", "TestType", "bbb", 3);
        SetupAsset(db, cas, c, "c.dat", "TestType", "ccc", 3);

        // b -> a (b depends on a), c -> b (c depends on b)
        db.GetDependencyGraph().AddEdge(b, a, nectar::DepKind::Hard);
        db.GetDependencyGraph().AddEdge(c, b, nectar::DepKind::Hard);

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};

        // Cook all
        nectar::CookRequest req{};
        req.assets = wax::Vector<nectar::AssetId>{alloc};
        req.assets.PushBack(a);
        req.assets.PushBack(b);
        req.assets.PushBack(c);
        req.platform = "pc";
        req.worker_count = 1;
        (void)pipe.CookAll(req);
        larvae::AssertEqual(cache.Count(), size_t{3});

        // Invalidate a → should cascade to b and c
        pipe.InvalidateCascade(a);
        larvae::AssertEqual(cache.Count(), size_t{0});
    });

    auto t11 = larvae::RegisterTest("NectarCookPipe", "CookAfterInvalidate", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_11"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};
        PassthroughCooker cooker;
        reg.Register(&cooker);

        auto id = MakeId(1);
        SetupAsset(db, cas, id, "test.dat", "TestType", "data", 4);

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};

        // Cook, invalidate, cook again
        (void)pipe.CookSingle(id, "pc");
        larvae::AssertEqual(cache.Count(), size_t{1});

        cache.Invalidate(id);
        larvae::AssertEqual(cache.Count(), size_t{0});

        auto result = pipe.CookSingle(id, "pc");
        larvae::AssertTrue(result.success);
        larvae::AssertEqual(cache.Count(), size_t{1});
    });

    // =========================================================================
    // CookKey
    // =========================================================================

    auto t12 = larvae::RegisterTest("NectarCookPipe", "CookKeyDeterminism", []() {
        nectar::ContentHash ih{0x1234, 0x5678};
        wax::Span<const nectar::ContentHash> no_deps{static_cast<const nectar::ContentHash*>(nullptr), size_t{0}};

        auto k1 = nectar::CookCache::BuildCookKey(ih, 1, "pc", no_deps);
        auto k2 = nectar::CookCache::BuildCookKey(ih, 1, "pc", no_deps);
        larvae::AssertTrue(k1 == k2);
    });

    auto t13 = larvae::RegisterTest("NectarCookPipe", "CookKeyChangesWithPlatform", []() {
        nectar::ContentHash ih{0x1234, 0x5678};
        wax::Span<const nectar::ContentHash> no_deps{static_cast<const nectar::ContentHash*>(nullptr), size_t{0}};

        auto k1 = nectar::CookCache::BuildCookKey(ih, 1, "pc", no_deps);
        auto k2 = nectar::CookCache::BuildCookKey(ih, 1, "ps5", no_deps);
        larvae::AssertTrue(k1 != k2);
    });

    auto t14 = larvae::RegisterTest("NectarCookPipe", "CookKeyChangesWithVersion", []() {
        nectar::ContentHash ih{0x1234, 0x5678};
        wax::Span<const nectar::ContentHash> no_deps{static_cast<const nectar::ContentHash*>(nullptr), size_t{0}};

        auto k1 = nectar::CookCache::BuildCookKey(ih, 1, "pc", no_deps);
        auto k2 = nectar::CookCache::BuildCookKey(ih, 2, "pc", no_deps);
        larvae::AssertTrue(k1 != k2);
    });

    auto t15 = larvae::RegisterTest("NectarCookPipe", "CookAllEmpty", []() {
        auto& alloc = GetCookAlloc();
        TempDir dir{"nectar_cook_test_15"};
        nectar::CasStore cas{alloc, dir.View()};
        nectar::AssetDatabase db{alloc};
        nectar::CookerRegistry reg{alloc};
        nectar::CookCache cache{alloc};

        nectar::CookPipeline pipe{alloc, reg, cas, db, cache};

        nectar::CookRequest req{};
        req.assets = wax::Vector<nectar::AssetId>{alloc};
        req.platform = "pc";
        req.worker_count = 1;

        auto out = pipe.CookAll(req);
        larvae::AssertEqual(out.total, size_t{0});
        larvae::AssertEqual(out.cooked, size_t{0});
    });

}
