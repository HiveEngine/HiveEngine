#include <larvae/larvae.h>
#include <nectar/server/asset_server.h>
#include <nectar/server/asset_event.h>
#include <comb/default_allocator.h>
#include <comb/new.h>
#include <cstring>

namespace {

    struct EvtAsset
    {
        int value;
    };

    class EvtLoader final : public nectar::AssetLoader<EvtAsset>
    {
    public:
        EvtAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            if (data.Size() < sizeof(int)) return nullptr;
            auto* a = comb::New<EvtAsset>(alloc);
            a->value = data.Read<int>(0);
            return a;
        }
        void Unload(EvtAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset) comb::Delete(alloc, asset);
        }
    };

    struct EvtAssetB
    {
        float x;
    };

    class EvtLoaderB final : public nectar::AssetLoader<EvtAssetB>
    {
    public:
        EvtAssetB* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            if (data.Size() < sizeof(float)) return nullptr;
            auto* a = comb::New<EvtAssetB>(alloc);
            a->x = data.Read<float>(0);
            return a;
        }
        void Unload(EvtAssetB* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset) comb::Delete(alloc, asset);
        }
    };

    auto& GetEvtAlloc()
    {
        static comb::ModuleAllocator alloc{"TestEvt", 4 * 1024 * 1024};
        return alloc.Get();
    }

    wax::ByteSpan IntSpan(uint8_t* buf, int v)
    {
        std::memcpy(buf, &v, sizeof(int));
        return wax::ByteSpan{buf, sizeof(int)};
    }

    wax::ByteSpan FloatSpan(uint8_t* buf, float v)
    {
        std::memcpy(buf, &v, sizeof(float));
        return wax::ByteSpan{buf, sizeof(float)};
    }

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarEvent", "LoadEmitsLoadedEvent", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto h = server.LoadFromMemory<EvtAsset>("evt_load", IntSpan(buf, 42));

        nectar::AssetEvent<EvtAsset> evt{};
        larvae::AssertTrue(server.PollEvents<EvtAsset>(evt));
        larvae::AssertEqual(static_cast<uint8_t>(evt.kind),
                           static_cast<uint8_t>(nectar::AssetEventKind::Loaded));
        larvae::AssertTrue(evt.handle == h.Raw());
    });

    auto t2 = larvae::RegisterTest("NectarEvent", "FailEmitsFailedEvent", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        // Too small → Load returns nullptr → Failed
        uint8_t buf[1] = {0};
        auto h = server.LoadFromMemory<EvtAsset>("evt_fail", wax::ByteSpan{buf, 1});

        nectar::AssetEvent<EvtAsset> evt{};
        larvae::AssertTrue(server.PollEvents<EvtAsset>(evt));
        larvae::AssertEqual(static_cast<uint8_t>(evt.kind),
                           static_cast<uint8_t>(nectar::AssetEventKind::Failed));
    });

    auto t3 = larvae::RegisterTest("NectarEvent", "GcEmitsUnloadedEvent", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        uint8_t buf[sizeof(int)];
        wax::Handle<EvtAsset> raw;
        {
            auto h = server.LoadFromMemory<EvtAsset>("evt_gc", IntSpan(buf, 10));
            raw = h.Raw();
            // Drain the Loaded event
            nectar::AssetEvent<EvtAsset> discard{};
            server.PollEvents<EvtAsset>(discard);
        }
        // h destroyed → ref_count 0
        server.Update(); // GC

        nectar::AssetEvent<EvtAsset> evt{};
        larvae::AssertTrue(server.PollEvents<EvtAsset>(evt));
        larvae::AssertEqual(static_cast<uint8_t>(evt.kind),
                           static_cast<uint8_t>(nectar::AssetEventKind::Unloaded));
        larvae::AssertTrue(evt.handle == raw);
    });

    auto t4 = larvae::RegisterTest("NectarEvent", "NoPollReturnsFalse", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        nectar::AssetEvent<EvtAsset> evt{};
        larvae::AssertFalse(server.PollEvents<EvtAsset>(evt));
    });

    auto t5 = larvae::RegisterTest("NectarEvent", "MultipleEventsInOrder", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        uint8_t buf1[sizeof(int)], buf2[sizeof(int)];
        auto h1 = server.LoadFromMemory<EvtAsset>("evt_ord1", IntSpan(buf1, 1));
        auto h2 = server.LoadFromMemory<EvtAsset>("evt_ord2", IntSpan(buf2, 2));

        nectar::AssetEvent<EvtAsset> evt1{}, evt2{};
        larvae::AssertTrue(server.PollEvents<EvtAsset>(evt1));
        larvae::AssertTrue(server.PollEvents<EvtAsset>(evt2));

        // Both should be Loaded, first = h1, second = h2
        larvae::AssertEqual(static_cast<uint8_t>(evt1.kind),
                           static_cast<uint8_t>(nectar::AssetEventKind::Loaded));
        larvae::AssertEqual(static_cast<uint8_t>(evt2.kind),
                           static_cast<uint8_t>(nectar::AssetEventKind::Loaded));
        larvae::AssertTrue(evt1.handle == h1.Raw());
        larvae::AssertTrue(evt2.handle == h2.Raw());
    });

    auto t6 = larvae::RegisterTest("NectarEvent", "EventsPerType", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loaderA;
        EvtLoaderB loaderB;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loaderA);
        server.RegisterLoader<EvtAssetB>(&loaderB);

        uint8_t bufA[sizeof(int)], bufB[sizeof(float)];
        auto hA = server.LoadFromMemory<EvtAsset>("type_a", IntSpan(bufA, 1));
        auto hB = server.LoadFromMemory<EvtAssetB>("type_b", FloatSpan(bufB, 2.0f));

        // Poll type A → should get only A events
        nectar::AssetEvent<EvtAsset> evtA{};
        larvae::AssertTrue(server.PollEvents<EvtAsset>(evtA));
        larvae::AssertTrue(evtA.handle == hA.Raw());

        // Poll type B → should get only B events
        nectar::AssetEvent<EvtAssetB> evtB{};
        larvae::AssertTrue(server.PollEvents<EvtAssetB>(evtB));
        larvae::AssertTrue(evtB.handle == hB.Raw());

        // No more events for either
        nectar::AssetEvent<EvtAsset> noA{};
        larvae::AssertFalse(server.PollEvents<EvtAsset>(noA));
        nectar::AssetEvent<EvtAssetB> noB{};
        larvae::AssertFalse(server.PollEvents<EvtAssetB>(noB));
    });

    auto t7 = larvae::RegisterTest("NectarEvent", "PollClearsQueue", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto h = server.LoadFromMemory<EvtAsset>("evt_clear", IntSpan(buf, 5));

        nectar::AssetEvent<EvtAsset> evt{};
        larvae::AssertTrue(server.PollEvents<EvtAsset>(evt));
        // Second poll should return false
        larvae::AssertFalse(server.PollEvents<EvtAsset>(evt));
    });

    auto t8 = larvae::RegisterTest("NectarEvent", "EventHandleValid", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto h = server.LoadFromMemory<EvtAsset>("evt_valid", IntSpan(buf, 99));

        nectar::AssetEvent<EvtAsset> evt{};
        server.PollEvents<EvtAsset>(evt);

        // Handle from event should match the loaded handle
        larvae::AssertEqual(evt.handle.index, h.Raw().index);
        larvae::AssertEqual(evt.handle.generation, h.Raw().generation);
    });

    auto t9 = larvae::RegisterTest("NectarEvent", "NoEventWhenCacheHit", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto h1 = server.LoadFromMemory<EvtAsset>("evt_dedup", IntSpan(buf, 7));

        // Drain the first Loaded event
        nectar::AssetEvent<EvtAsset> discard{};
        server.PollEvents<EvtAsset>(discard);

        // Load same path again → cache hit, no new event
        auto h2 = server.LoadFromMemory<EvtAsset>("evt_dedup", IntSpan(buf, 7));

        nectar::AssetEvent<EvtAsset> evt{};
        larvae::AssertFalse(server.PollEvents<EvtAsset>(evt));
    });

    auto t10 = larvae::RegisterTest("NectarEvent", "ReloadEmitsReloadedEvent", []() {
        auto& alloc = GetEvtAlloc();
        EvtLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<EvtAsset>(&loader);

        uint8_t buf1[sizeof(int)], buf2[sizeof(int)];
        auto h = server.LoadFromMemory<EvtAsset>("evt_reload", IntSpan(buf1, 10));

        // Drain the Loaded event
        nectar::AssetEvent<EvtAsset> discard{};
        server.PollEvents<EvtAsset>(discard);

        // Reload with new data
        bool ok = server.Reload<EvtAsset>(h.Raw(), IntSpan(buf2, 20));
        larvae::AssertTrue(ok);

        // Should emit Reloaded
        nectar::AssetEvent<EvtAsset> evt{};
        larvae::AssertTrue(server.PollEvents<EvtAsset>(evt));
        larvae::AssertEqual(static_cast<uint8_t>(evt.kind),
                           static_cast<uint8_t>(nectar::AssetEventKind::Reloaded));

        // Asset should have new value
        auto* asset = server.Get(h);
        larvae::AssertNotNull(asset);
        larvae::AssertEqual(asset->value, 20);
    });

} // namespace
