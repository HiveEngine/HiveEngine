#include <larvae/larvae.h>
#include <nectar/server/asset_server.h>
#include <nectar/server/asset_event.h>
#include <comb/default_allocator.h>
#include <comb/new.h>
#include <cstring>

namespace {

    struct GcAsset
    {
        int value;
    };

    class GcLoader final : public nectar::AssetLoader<GcAsset>
    {
    public:
        GcAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            if (data.Size() < sizeof(int)) return nullptr;
            auto* a = comb::New<GcAsset>(alloc);
            a->value = data.Read<int>(0);
            return a;
        }
        void Unload(GcAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset) comb::Delete(alloc, asset);
        }
        size_t SizeOf(const GcAsset*) const override { return 1024; }
    };

    auto& GetGcAlloc()
    {
        static comb::ModuleAllocator alloc{"TestGcBudget", 4 * 1024 * 1024};
        return alloc.Get();
    }

    wax::ByteSpan IntData(uint8_t* buf, int v)
    {
        std::memcpy(buf, &v, sizeof(int));
        return wax::ByteSpan{buf, sizeof(int)};
    }

    // Drain all events for type
    void DrainAllEvents(nectar::AssetServer& server)
    {
        nectar::AssetEvent<GcAsset> evt{};
        while (server.PollEvents<GcAsset>(evt)) {}
    }

    // =========================================================================
    // GC Grace Period
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarGcBudget", "GcGracePeriodDelaysUnload", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(3);

        uint8_t buf[sizeof(int)];
        nectar::WeakHandle<GcAsset> weak;
        {
            auto h = server.LoadFromMemory<GcAsset>("gc_grace", IntData(buf, 1));
            weak = h.MakeWeak();
        }
        // ref_count = 0

        // Frame 1: countdown starts (set to 3)
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});

        // Frame 2: countdown = 2
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});

        // Frame 3: countdown = 1
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});

        // Frame 4: countdown = 0 → unloaded
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{0});
    });

    auto t2 = larvae::RegisterTest("NectarGcBudget", "GcGracePeriodResetOnReuse", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(3);

        uint8_t buf[sizeof(int)];
        auto h = server.LoadFromMemory<GcAsset>("gc_reuse", IntData(buf, 2));
        nectar::WeakHandle<GcAsset> weak = h.MakeWeak();

        // Drop the handle → ref_count = 0
        server.Release(h);

        // Frame 1-2: countdown starts
        server.Update();
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});

        // Re-acquire via Lock → ref_count > 0
        auto locked = server.Lock(weak);
        larvae::AssertFalse(locked.IsNull());

        // Next update should reset countdown (ref > 0)
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});

        // Drop again
        locked = nectar::StrongHandle<GcAsset>{};

        // Should need 3 more full frames
        server.Update(); // start countdown
        server.Update(); // 2
        server.Update(); // 1
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});
        server.Update(); // 0 → unloaded
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{0});
    });

    auto t3 = larvae::RegisterTest("NectarGcBudget", "GcImmediateWhenGraceZero", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(0);

        uint8_t buf[sizeof(int)];
        {
            auto h = server.LoadFromMemory<GcAsset>("gc_imm", IntData(buf, 3));
        }

        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{0});
    });

    auto t4 = larvae::RegisterTest("NectarGcBudget", "PersistentExemptFromGc", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(0);

        uint8_t buf[sizeof(int)];
        auto h = server.LoadFromMemory<GcAsset>("gc_pers", IntData(buf, 4));
        server.SetPersistent(h, true);

        server.Release(h);
        server.Update();
        // Should NOT be collected (persistent)
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});
    });

    auto t5 = larvae::RegisterTest("NectarGcBudget", "PersistentCanBeCleared", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(0);

        uint8_t buf[sizeof(int)];
        auto h = server.LoadFromMemory<GcAsset>("gc_pers_clr", IntData(buf, 5));
        auto raw = h.Raw();
        server.SetPersistent(h, true);

        server.Release(h);
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});

        // Clear persistent via weak → need to re-acquire
        auto relocked = server.Lock(nectar::WeakHandle<GcAsset>{raw});
        server.SetPersistent(relocked, false);
        relocked = nectar::StrongHandle<GcAsset>{};

        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{0});
    });

    // =========================================================================
    // Budget
    // =========================================================================

    auto t6 = larvae::RegisterTest("NectarGcBudget", "BudgetTracksBytesUsed", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader; // SizeOf returns 1024
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);

        uint8_t buf1[sizeof(int)], buf2[sizeof(int)];
        auto h1 = server.LoadFromMemory<GcAsset>("bud1", IntData(buf1, 1));
        auto h2 = server.LoadFromMemory<GcAsset>("bud2", IntData(buf2, 2));

        larvae::AssertEqual(server.GetBytesUsed<GcAsset>(), size_t{2048});
    });

    auto t7 = larvae::RegisterTest("NectarGcBudget", "BudgetTriggersAggressiveGc", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader; // 1024 per asset
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(100); // large grace → normally wouldn't GC soon
        server.SetBudget<GcAsset>(2048); // budget = 2KB

        uint8_t buf1[sizeof(int)], buf2[sizeof(int)], buf3[sizeof(int)];
        auto h1 = server.LoadFromMemory<GcAsset>("bud_agg1", IntData(buf1, 1));
        auto h2 = server.LoadFromMemory<GcAsset>("bud_agg2", IntData(buf2, 2));
        auto h3 = server.LoadFromMemory<GcAsset>("bud_agg3", IntData(buf3, 3));

        DrainAllEvents(server);

        // 3 assets × 1024 = 3072 > budget 2048
        // Drop h1 and h2 → ref_count = 0
        server.Release(h1);
        server.Release(h2);

        // Update should trigger aggressive GC (ignoring grace period) for unreferenced
        server.Update();

        // At least one should be collected to get under budget
        larvae::AssertTrue(server.GetBytesUsed<GcAsset>() <= 2048);
        // h3 still alive (referenced)
        larvae::AssertTrue(server.IsReady(h3));
    });

    auto t8 = larvae::RegisterTest("NectarGcBudget", "BudgetZeroMeansUnlimited", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(100);
        // budget_ = 0 by default → unlimited

        uint8_t buf[sizeof(int)];
        {
            auto h = server.LoadFromMemory<GcAsset>("bud_unlim", IntData(buf, 1));
        }

        // Grace period = 100, budget unlimited → should NOT GC on first update
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});
    });

    auto t9 = larvae::RegisterTest("NectarGcBudget", "GcCountdownDecrementsPerUpdate", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(5);

        uint8_t buf[sizeof(int)];
        {
            auto h = server.LoadFromMemory<GcAsset>("gc_dec", IntData(buf, 1));
        }

        // 5 frames with countdown, 6th frame unloads
        for (int i = 0; i < 5; ++i)
        {
            server.Update();
            larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});
        }
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{0});
    });

    auto t10 = larvae::RegisterTest("NectarGcBudget", "MultipleAssetsGcIndependent", []() {
        auto& alloc = GetGcAlloc();
        GcLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<GcAsset>(&loader);
        server.SetGcGraceFrames(2);

        uint8_t buf1[sizeof(int)], buf2[sizeof(int)];
        nectar::WeakHandle<GcAsset> w1, w2;
        {
            auto h1 = server.LoadFromMemory<GcAsset>("gc_ind1", IntData(buf1, 1));
            w1 = h1.MakeWeak();
        }

        // h1 ref_count = 0, start grace
        server.Update(); // h1 countdown starts
        server.Update(); // h1 countdown = 1

        // Now load h2 and immediately drop
        {
            auto h2 = server.LoadFromMemory<GcAsset>("gc_ind2", IntData(buf2, 2));
            w2 = h2.MakeWeak();
        }

        server.Update(); // h1 countdown = 0 → unloaded, h2 starts countdown

        // h1 should be gone, h2 still counting down
        auto l1 = server.Lock(w1);
        larvae::AssertTrue(l1.IsNull());

        auto l2 = server.Lock(w2);
        larvae::AssertFalse(l2.IsNull());
    });

} // namespace
