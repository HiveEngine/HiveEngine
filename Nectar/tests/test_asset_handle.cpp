#include <larvae/larvae.h>
#include <nectar/server/asset_server.h>
#include <nectar/server/asset_loader.h>
#include <comb/default_allocator.h>
#include <comb/new.h>
#include <cstring>

namespace {

    struct HandleTestAsset
    {
        int val;
    };

    class HandleTestLoader final : public nectar::AssetLoader<HandleTestAsset>
    {
    public:
        HandleTestAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            if (data.Size() < sizeof(int)) return nullptr;
            auto* a = comb::New<HandleTestAsset>(alloc);
            a->val = data.Read<int>(0);
            return a;
        }
        void Unload(HandleTestAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset) comb::Delete(alloc, asset);
        }
    };

    auto& GetHandleTestAlloc()
    {
        static comb::ModuleAllocator alloc{"TestHandle", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // WeakHandle
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarAssetHandle", "WeakDefaultIsNull", []() {
        nectar::WeakHandle<int> w{};
        larvae::AssertTrue(w.IsNull());
    });

    auto t2 = larvae::RegisterTest("NectarAssetHandle", "WeakInvalidIsNull", []() {
        auto w = nectar::WeakHandle<int>::Invalid();
        larvae::AssertTrue(w.IsNull());
    });

    auto t3 = larvae::RegisterTest("NectarAssetHandle", "WeakEquality", []() {
        nectar::WeakHandle<int> a{};
        nectar::WeakHandle<int> b{};
        larvae::AssertTrue(a == b);
        larvae::AssertFalse(a != b);
    });

    // =========================================================================
    // StrongHandle — null handles
    // =========================================================================

    auto t4 = larvae::RegisterTest("NectarAssetHandle", "StrongDefaultIsNull", []() {
        nectar::StrongHandle<int> h{};
        larvae::AssertTrue(h.IsNull());
    });

    auto t5 = larvae::RegisterTest("NectarAssetHandle", "StrongNullDestructorSafe", []() {
        {
            nectar::StrongHandle<int> h{};
        }
        // Should not crash
        larvae::AssertTrue(true);
    });

    auto t6 = larvae::RegisterTest("NectarAssetHandle", "StrongMoveFromNull", []() {
        nectar::StrongHandle<int> a{};
        nectar::StrongHandle<int> b{static_cast<nectar::StrongHandle<int>&&>(a)};
        larvae::AssertTrue(a.IsNull());
        larvae::AssertTrue(b.IsNull());
    });

    // =========================================================================
    // StrongHandle — RAII ref counting (needs AssetServer)
    // =========================================================================

    auto t7 = larvae::RegisterTest("NectarAssetHandle", "CopyIncrementsRefCount", []() {
        auto& alloc = GetHandleTestAlloc();
        HandleTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<HandleTestAsset>(&loader);

        int v = 42;
        uint8_t buf[sizeof(int)];
        std::memcpy(buf, &v, sizeof(int));
        wax::ByteSpan data{buf, sizeof(buf)};

        auto h1 = server.LoadFromMemory<HandleTestAsset>("test", data);
        larvae::AssertFalse(h1.IsNull());

        {
            auto h2 = h1; // copy
            larvae::AssertFalse(h2.IsNull());
            larvae::AssertTrue(h1 == h2);
            // Both h1 and h2 alive — asset should be alive
            larvae::AssertTrue(server.IsReady(h1));
        }
        // h2 destroyed, but h1 still holds ref
        larvae::AssertTrue(server.IsReady(h1));
    });

    auto t8 = larvae::RegisterTest("NectarAssetHandle", "MoveDoesNotIncrementRef", []() {
        auto& alloc = GetHandleTestAlloc();
        HandleTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<HandleTestAsset>(&loader);

        int v = 10;
        uint8_t buf[sizeof(int)];
        std::memcpy(buf, &v, sizeof(int));

        auto h1 = server.LoadFromMemory<HandleTestAsset>("move_test", wax::ByteSpan{buf, sizeof(buf)});
        auto raw = h1.Raw();

        auto h2 = static_cast<nectar::StrongHandle<HandleTestAsset>&&>(h1);
        larvae::AssertTrue(h1.IsNull());
        larvae::AssertFalse(h2.IsNull());
        larvae::AssertTrue(h2.Raw() == raw);
    });

    auto t9 = larvae::RegisterTest("NectarAssetHandle", "DestructorDecrementsRef", []() {
        auto& alloc = GetHandleTestAlloc();
        HandleTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<HandleTestAsset>(&loader);

        int v = 5;
        uint8_t buf[sizeof(int)];
        std::memcpy(buf, &v, sizeof(int));

        nectar::WeakHandle<HandleTestAsset> weak;
        {
            auto strong = server.LoadFromMemory<HandleTestAsset>("destr_test", wax::ByteSpan{buf, sizeof(buf)});
            weak = strong.MakeWeak();
            larvae::AssertFalse(weak.IsNull());
        }
        // strong destroyed, ref count = 0. After Update, slot should be GC'd.
        server.Update();

        auto locked = server.Lock(weak);
        larvae::AssertTrue(locked.IsNull());
    });

    auto t10 = larvae::RegisterTest("NectarAssetHandle", "MakeWeakPreservesHandle", []() {
        auto& alloc = GetHandleTestAlloc();
        HandleTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<HandleTestAsset>(&loader);

        int v = 1;
        uint8_t buf[sizeof(int)];
        std::memcpy(buf, &v, sizeof(int));

        auto strong = server.LoadFromMemory<HandleTestAsset>("weak_test", wax::ByteSpan{buf, sizeof(buf)});
        auto weak = strong.MakeWeak();

        larvae::AssertTrue(strong.Raw() == weak.raw);
    });

    auto t11 = larvae::RegisterTest("NectarAssetHandle", "SelfAssignment", []() {
        auto& alloc = GetHandleTestAlloc();
        HandleTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<HandleTestAsset>(&loader);

        int v = 7;
        uint8_t buf[sizeof(int)];
        std::memcpy(buf, &v, sizeof(int));

        auto h = server.LoadFromMemory<HandleTestAsset>("self_test", wax::ByteSpan{buf, sizeof(buf)});
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
        h = h; // self-assignment
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
        larvae::AssertFalse(h.IsNull());
        larvae::AssertTrue(server.IsReady(h));
    });

    auto t12 = larvae::RegisterTest("NectarAssetHandle", "CopyAssignmentReleasesOld", []() {
        auto& alloc = GetHandleTestAlloc();
        HandleTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<HandleTestAsset>(&loader);

        int v1 = 1, v2 = 2;
        uint8_t buf1[sizeof(int)], buf2[sizeof(int)];
        std::memcpy(buf1, &v1, sizeof(int));
        std::memcpy(buf2, &v2, sizeof(int));

        auto h1 = server.LoadFromMemory<HandleTestAsset>("ca1", wax::ByteSpan{buf1, sizeof(buf1)});
        auto h2 = server.LoadFromMemory<HandleTestAsset>("ca2", wax::ByteSpan{buf2, sizeof(buf2)});

        auto weak1 = h1.MakeWeak();
        h1 = h2; // h1 now points to h2's asset, old asset ref decremented

        // After GC, the first asset should be gone (ref count 0)
        server.Update();
        auto locked = server.Lock(weak1);
        larvae::AssertTrue(locked.IsNull());
    });

    auto t13 = larvae::RegisterTest("NectarAssetHandle", "MoveAssignmentSafe", []() {
        auto& alloc = GetHandleTestAlloc();
        HandleTestLoader loader;
        nectar::AssetServer server{alloc};
        server.RegisterLoader<HandleTestAsset>(&loader);

        int v = 3;
        uint8_t buf[sizeof(int)];
        std::memcpy(buf, &v, sizeof(int));

        auto h1 = server.LoadFromMemory<HandleTestAsset>("ma_test", wax::ByteSpan{buf, sizeof(buf)});
        nectar::StrongHandle<HandleTestAsset> h2;
        h2 = static_cast<nectar::StrongHandle<HandleTestAsset>&&>(h1);

        larvae::AssertTrue(h1.IsNull());
        larvae::AssertFalse(h2.IsNull());
        larvae::AssertTrue(server.IsReady(h2));
    });

}
