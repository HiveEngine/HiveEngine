#include <larvae/larvae.h>
#include <nectar/server/asset_server.h>
#include <nectar/server/asset_loader.h>
#include <comb/default_allocator.h>
#include <comb/new.h>
#include <cstring>

namespace {

    struct ServerTestAsset
    {
        int value;
    };

    class ServerTestLoader final : public nectar::AssetLoader<ServerTestAsset>
    {
    public:
        ServerTestAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            if (data.Size() < sizeof(int)) return nullptr;
            auto* a = comb::New<ServerTestAsset>(alloc);
            a->value = data.Read<int>(0);
            return a;
        }
        void Unload(ServerTestAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset) comb::Delete(alloc, asset);
        }
    };

    // A different asset type to test multi-type support
    struct OtherAsset
    {
        float x;
    };

    class OtherLoader final : public nectar::AssetLoader<OtherAsset>
    {
    public:
        OtherAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            if (data.Size() < sizeof(float)) return nullptr;
            auto* a = comb::New<OtherAsset>(alloc);
            a->x = data.Read<float>(0);
            return a;
        }
        void Unload(OtherAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset) comb::Delete(alloc, asset);
        }
    };

    auto& GetServerTestAlloc()
    {
        static comb::ModuleAllocator alloc{"TestServer", 8 * 1024 * 1024};
        return alloc.Get();
    }

    wax::ByteSpan MakeIntSpan(uint8_t* buf, int value)
    {
        std::memcpy(buf, &value, sizeof(int));
        return wax::ByteSpan{buf, sizeof(int)};
    }

    wax::ByteSpan MakeFloatSpan(uint8_t* buf, float value)
    {
        std::memcpy(buf, &value, sizeof(float));
        return wax::ByteSpan{buf, sizeof(float)};
    }

    // =========================================================================
    // Registration
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarAssetServer", "RegisterLoader", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);
        // Should not crash
        larvae::AssertTrue(true);
    });

    // =========================================================================
    // LoadFromMemory
    // =========================================================================

    auto t2 = larvae::RegisterTest("NectarAssetServer", "LoadFromMemoryValid", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 42);

        auto handle = server.LoadFromMemory<ServerTestAsset>("test_asset", data);
        larvae::AssertFalse(handle.IsNull());
        larvae::AssertTrue(server.IsReady(handle));
    });

    auto t3 = larvae::RegisterTest("NectarAssetServer", "GetReturnsLoadedAsset", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 123);

        auto handle = server.LoadFromMemory<ServerTestAsset>("get_test", data);
        auto* asset = server.Get(handle);
        larvae::AssertNotNull(asset);
        larvae::AssertEqual(asset->value, 123);
    });

    // =========================================================================
    // Status
    // =========================================================================

    auto t4 = larvae::RegisterTest("NectarAssetServer", "StatusIsReady", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 1);

        auto handle = server.LoadFromMemory<ServerTestAsset>("status_test", data);
        larvae::AssertEqual(
            static_cast<uint8_t>(server.GetStatus(handle)),
            static_cast<uint8_t>(nectar::AssetStatus::Ready));
    });

    auto t5 = larvae::RegisterTest("NectarAssetServer", "NullHandleStatus", []() {
        auto& alloc = GetServerTestAlloc();
        nectar::AssetServer server{alloc};
        nectar::StrongHandle<ServerTestAsset> null{};
        larvae::AssertEqual(
            static_cast<uint8_t>(server.GetStatus(null)),
            static_cast<uint8_t>(nectar::AssetStatus::NotLoaded));
    });

    // =========================================================================
    // Dedup (same path)
    // =========================================================================

    auto t6 = larvae::RegisterTest("NectarAssetServer", "LoadSamePathReturnsSameHandle", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 99);

        auto h1 = server.LoadFromMemory<ServerTestAsset>("dedup", data);
        auto h2 = server.LoadFromMemory<ServerTestAsset>("dedup", data);

        larvae::AssertTrue(h1.Raw() == h2.Raw());
    });

    auto t7 = larvae::RegisterTest("NectarAssetServer", "LoadDifferentPathsDifferentHandles", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 1);

        auto h1 = server.LoadFromMemory<ServerTestAsset>("path_a", data);
        auto h2 = server.LoadFromMemory<ServerTestAsset>("path_b", data);

        larvae::AssertFalse(h1.Raw() == h2.Raw());
    });

    // =========================================================================
    // Failed loads
    // =========================================================================

    auto t8 = larvae::RegisterTest("NectarAssetServer", "LoadFailedStatus", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        // Too short — loader returns nullptr
        uint8_t buf[1] = {0};
        wax::ByteSpan data{buf, 1};

        auto handle = server.LoadFromMemory<ServerTestAsset>("fail_test", data);
        larvae::AssertFalse(handle.IsNull());
        larvae::AssertEqual(
            static_cast<uint8_t>(server.GetStatus(handle)),
            static_cast<uint8_t>(nectar::AssetStatus::Failed));
    });

    auto t9 = larvae::RegisterTest("NectarAssetServer", "GetFailedReturnsPlaceholder", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        ServerTestAsset placeholder{-1};
        server.RegisterPlaceholder<ServerTestAsset>(&placeholder);

        uint8_t buf[1] = {0};
        auto handle = server.LoadFromMemory<ServerTestAsset>("fail_ph", wax::ByteSpan{buf, 1});

        auto* got = server.Get(handle);
        larvae::AssertNotNull(got);
        larvae::AssertEqual(got->value, -1);
    });

    auto t10 = larvae::RegisterTest("NectarAssetServer", "GetNullHandleReturnsPlaceholder", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        ServerTestAsset placeholder{-99};
        server.RegisterPlaceholder<ServerTestAsset>(&placeholder);

        nectar::StrongHandle<ServerTestAsset> null{};
        auto* got = server.Get(null);
        larvae::AssertNotNull(got);
        larvae::AssertEqual(got->value, -99);
    });

    // =========================================================================
    // No loader
    // =========================================================================

    auto t11 = larvae::RegisterTest("NectarAssetServer", "LoadNoLoaderFails", []() {
        auto& alloc = GetServerTestAlloc();
        nectar::AssetServer server{alloc};
        // No loader registered

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 1);

        auto handle = server.LoadFromMemory<ServerTestAsset>("no_loader", data);
        larvae::AssertFalse(handle.IsNull());
        larvae::AssertEqual(
            static_cast<uint8_t>(server.GetStatus(handle)),
            static_cast<uint8_t>(nectar::AssetStatus::Failed));

        auto* err = server.GetError(handle);
        larvae::AssertNotNull(err);
        larvae::AssertEqual(
            static_cast<uint8_t>(err->code),
            static_cast<uint8_t>(nectar::AssetError::NoLoader));
    });

    // =========================================================================
    // Placeholder
    // =========================================================================

    auto t12 = larvae::RegisterTest("NectarAssetServer", "PlaceholderRegistration", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        ServerTestAsset ph{666};
        server.RegisterPlaceholder<ServerTestAsset>(&ph);

        // Get on null handle returns placeholder
        nectar::StrongHandle<ServerTestAsset> null{};
        auto* got = server.Get(null);
        larvae::AssertNotNull(got);
        larvae::AssertEqual(got->value, 666);
    });

    // =========================================================================
    // Ref counting through AssetServer
    // =========================================================================

    auto t13 = larvae::RegisterTest("NectarAssetServer", "MultipleStrongHandlesKeepAlive", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 55);

        auto h1 = server.LoadFromMemory<ServerTestAsset>("multi_ref", data);
        nectar::WeakHandle<ServerTestAsset> weak = h1.MakeWeak();

        {
            auto h2 = h1; // copy — ref count = 2
            auto h3 = h1; // copy — ref count = 3

            server.Update();
            // h1 still alive, so GC shouldn't collect
            auto locked = server.Lock(weak);
            larvae::AssertFalse(locked.IsNull());
        }
        // h2, h3 destroyed — ref count = 1 (h1 still alive)
        server.Update();
        auto locked = server.Lock(weak);
        larvae::AssertFalse(locked.IsNull());
    });

    // =========================================================================
    // Weak handle Lock
    // =========================================================================

    auto t14 = larvae::RegisterTest("NectarAssetServer", "WeakLockWhileAlive", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 77);

        auto strong = server.LoadFromMemory<ServerTestAsset>("weak_lock", data);
        auto weak = strong.MakeWeak();

        auto locked = server.Lock(weak);
        larvae::AssertFalse(locked.IsNull());
        larvae::AssertTrue(server.IsReady(locked));
    });

    auto t15 = larvae::RegisterTest("NectarAssetServer", "WeakLockAfterUnload", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 88);

        nectar::WeakHandle<ServerTestAsset> weak;
        {
            auto strong = server.LoadFromMemory<ServerTestAsset>("weak_dead", data);
            weak = strong.MakeWeak();
        }
        // strong destroyed, ref count = 0
        server.Update(); // GC

        auto locked = server.Lock(weak);
        larvae::AssertTrue(locked.IsNull());
    });

    auto t16 = larvae::RegisterTest("NectarAssetServer", "WeakLockNull", []() {
        auto& alloc = GetServerTestAlloc();
        nectar::AssetServer server{alloc};

        nectar::WeakHandle<ServerTestAsset> null{};
        auto locked = server.Lock(null);
        larvae::AssertTrue(locked.IsNull());
    });

    // =========================================================================
    // Update / GC
    // =========================================================================

    auto t17 = larvae::RegisterTest("NectarAssetServer", "UpdateCollectsZeroRefAssets", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 10);

        {
            auto h = server.LoadFromMemory<ServerTestAsset>("gc_test", data);
            larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});
        }
        // h destroyed → ref count 0
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{0});
    });

    auto t18 = larvae::RegisterTest("NectarAssetServer", "UpdateKeepsReferencedAssets", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 20);

        auto h = server.LoadFromMemory<ServerTestAsset>("gc_keep", data);
        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{1});
        larvae::AssertTrue(server.IsReady(h));
    });

    // =========================================================================
    // Multi-type support
    // =========================================================================

    auto t19 = larvae::RegisterTest("NectarAssetServer", "MultiTypeSupport", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader intLoader;
        OtherLoader floatLoader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&intLoader);
        server.RegisterLoader<OtherAsset>(&floatLoader);

        uint8_t intBuf[sizeof(int)];
        auto intData = MakeIntSpan(intBuf, 42);

        uint8_t floatBuf[sizeof(float)];
        auto floatData = MakeFloatSpan(floatBuf, 3.14f);

        auto h1 = server.LoadFromMemory<ServerTestAsset>("int_asset", intData);
        auto h2 = server.LoadFromMemory<OtherAsset>("float_asset", floatData);

        larvae::AssertTrue(server.IsReady(h1));
        larvae::AssertTrue(server.IsReady(h2));

        auto* a1 = server.Get(h1);
        auto* a2 = server.Get(h2);
        larvae::AssertNotNull(a1);
        larvae::AssertNotNull(a2);
        larvae::AssertEqual(a1->value, 42);
        larvae::AssertFloatEqual(a2->x, 3.14f);
    });

    // =========================================================================
    // Release
    // =========================================================================

    auto t20 = larvae::RegisterTest("NectarAssetServer", "ExplicitRelease", []() {
        auto& alloc = GetServerTestAlloc();
        ServerTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<ServerTestAsset>(&loader);

        uint8_t buf[sizeof(int)];
        auto data = MakeIntSpan(buf, 5);

        auto h = server.LoadFromMemory<ServerTestAsset>("release_test", data);
        larvae::AssertFalse(h.IsNull());

        server.Release(h);
        larvae::AssertTrue(h.IsNull());

        server.Update();
        larvae::AssertEqual(server.GetTotalAssetCount(), size_t{0});
    });

}
