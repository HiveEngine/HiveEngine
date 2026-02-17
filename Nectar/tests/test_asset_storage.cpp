#include <larvae/larvae.h>
#include <nectar/server/asset_storage.h>
#include <nectar/server/asset_loader.h>
#include <comb/default_allocator.h>
#include <comb/new.h>

namespace {

    struct DummyAsset
    {
        int id;
    };

    class DummyLoader final : public nectar::AssetLoader<DummyAsset>
    {
    public:
        DummyAsset* Load(wax::ByteSpan, comb::DefaultAllocator& alloc) override
        {
            auto* a = comb::New<DummyAsset>(alloc);
            a->id = 777;
            return a;
        }
        void Unload(DummyAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset) comb::Delete(alloc, asset);
        }
    };

    auto& GetStorageAlloc()
    {
        static comb::ModuleAllocator alloc{"TestStorage", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // Allocation
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarAssetStorage", "AllocateSlotReturnsValid", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};

        auto handle = storage.AllocateSlot();
        larvae::AssertFalse(handle.IsNull());
        larvae::AssertEqual(storage.Count(), size_t{1});
    });

    auto t2 = larvae::RegisterTest("NectarAssetStorage", "AllocateMultiple", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};

        for (int i = 0; i < 8; ++i)
        {
            auto h = storage.AllocateSlot();
            larvae::AssertFalse(h.IsNull());
        }
        larvae::AssertEqual(storage.Count(), size_t{8});
    });

    auto t3 = larvae::RegisterTest("NectarAssetStorage", "PoolFull", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 2};

        auto h1 = storage.AllocateSlot();
        auto h2 = storage.AllocateSlot();
        auto h3 = storage.AllocateSlot();
        larvae::AssertFalse(h1.IsNull());
        larvae::AssertFalse(h2.IsNull());
        larvae::AssertTrue(h3.IsNull());
    });

    // =========================================================================
    // Asset set/get
    // =========================================================================

    auto t4 = larvae::RegisterTest("NectarAssetStorage", "SetAndGetAsset", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};

        auto handle = storage.AllocateSlot();
        auto* asset = comb::New<DummyAsset>(alloc);
        asset->id = 42;

        storage.SetAsset(handle, asset);
        storage.SetStatus(handle.index, nectar::AssetStatus::Ready);

        auto* got = storage.GetAsset(handle);
        larvae::AssertNotNull(got);
        larvae::AssertEqual(got->id, 42);

        comb::Delete(alloc, asset);
    });

    auto t5 = larvae::RegisterTest("NectarAssetStorage", "GetWithInvalidGeneration", []() {
        auto& alloc = GetStorageAlloc();
        DummyLoader loader;
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};
        storage.SetLoader(&loader);

        auto handle = storage.AllocateSlot();
        auto* asset = comb::New<DummyAsset>(alloc);
        storage.SetAsset(handle, asset);
        storage.SetStatus(handle.index, nectar::AssetStatus::Ready);

        // Unload to increment generation
        storage.UnloadSlot(handle.index, handle.generation);

        auto* got = storage.GetAsset(handle);
        larvae::AssertNull(got);
    });

    // =========================================================================
    // Ref counting
    // =========================================================================

    auto t6 = larvae::RegisterTest("NectarAssetStorage", "RefCountStartsAtZero", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};

        auto handle = storage.AllocateSlot();
        larvae::AssertEqual(storage.GetRefCount(handle.index), uint32_t{0});
    });

    auto t7 = larvae::RegisterTest("NectarAssetStorage", "IncrementDecrementRef", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};

        auto handle = storage.AllocateSlot();
        storage.IncrementRef(handle.index);
        storage.IncrementRef(handle.index);
        larvae::AssertEqual(storage.GetRefCount(handle.index), uint32_t{2});

        storage.DecrementRef(handle.index);
        larvae::AssertEqual(storage.GetRefCount(handle.index), uint32_t{1});
    });

    // =========================================================================
    // Status transitions
    // =========================================================================

    auto t8 = larvae::RegisterTest("NectarAssetStorage", "StatusTransitions", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};

        auto handle = storage.AllocateSlot();
        larvae::AssertEqual(
            static_cast<uint8_t>(storage.GetStatus(handle.index)),
            static_cast<uint8_t>(nectar::AssetStatus::NotLoaded));

        storage.SetStatus(handle.index, nectar::AssetStatus::Loading);
        larvae::AssertEqual(
            static_cast<uint8_t>(storage.GetStatus(handle.index)),
            static_cast<uint8_t>(nectar::AssetStatus::Loading));

        storage.SetStatus(handle.index, nectar::AssetStatus::Ready);
        larvae::AssertEqual(
            static_cast<uint8_t>(storage.GetStatus(handle.index)),
            static_cast<uint8_t>(nectar::AssetStatus::Ready));
    });

    // =========================================================================
    // Error info
    // =========================================================================

    auto t9 = larvae::RegisterTest("NectarAssetStorage", "ErrorInfo", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};

        auto handle = storage.AllocateSlot();
        storage.SetError(handle.index, nectar::AssetErrorInfo{nectar::AssetError::FileNotFound, wax::String<>{}});

        auto* err = storage.GetError(handle.index);
        larvae::AssertNotNull(err);
        larvae::AssertEqual(
            static_cast<uint8_t>(err->code),
            static_cast<uint8_t>(nectar::AssetError::FileNotFound));
    });

    // =========================================================================
    // Placeholder
    // =========================================================================

    auto t10 = larvae::RegisterTest("NectarAssetStorage", "PlaceholderReturnedWhenNotReady", []() {
        auto& alloc = GetStorageAlloc();
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};

        DummyAsset placeholder{-1};
        storage.SetPlaceholder(&placeholder);

        auto handle = storage.AllocateSlot();
        storage.SetStatus(handle.index, nectar::AssetStatus::Loading);

        auto* got = storage.GetAssetOrPlaceholder(handle);
        larvae::AssertNotNull(got);
        larvae::AssertEqual(got->id, -1);
    });

    // =========================================================================
    // Destroy and reuse
    // =========================================================================

    auto t11 = larvae::RegisterTest("NectarAssetStorage", "DestroyAndReuseSlot", []() {
        auto& alloc = GetStorageAlloc();
        DummyLoader loader;
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 2};
        storage.SetLoader(&loader);

        auto h1 = storage.AllocateSlot();
        auto h2 = storage.AllocateSlot();
        larvae::AssertFalse(h2.IsNull());
        larvae::AssertEqual(storage.Count(), size_t{2});

        // Full
        auto h3 = storage.AllocateSlot();
        larvae::AssertTrue(h3.IsNull());

        // Unload first slot
        storage.UnloadSlot(h1.index, h1.generation);
        larvae::AssertEqual(storage.Count(), size_t{1});

        // Can allocate again
        auto h4 = storage.AllocateSlot();
        larvae::AssertFalse(h4.IsNull());
        larvae::AssertEqual(storage.Count(), size_t{2});

        // Old handle is stale (generation incremented)
        larvae::AssertFalse(storage.IsHandleValid(h1.index, h1.generation));
    });

    // =========================================================================
    // Garbage collection
    // =========================================================================

    auto t12 = larvae::RegisterTest("NectarAssetStorage", "CollectGarbage", []() {
        auto& alloc = GetStorageAlloc();
        DummyLoader loader;
        nectar::AssetStorageFor<DummyAsset> storage{alloc, 8};
        storage.SetLoader(&loader);

        // Allocate, load, set ready, but leave ref count at 0
        auto h = storage.AllocateSlot();
        auto* asset = loader.Load(wax::ByteSpan{}, alloc);
        storage.SetAsset(h, asset);
        storage.SetStatus(h.index, nectar::AssetStatus::Ready);

        larvae::AssertEqual(storage.GetRefCount(h.index), uint32_t{0});
        larvae::AssertEqual(storage.Count(), size_t{1});

        size_t collected = storage.CollectGarbage(0);
        larvae::AssertEqual(collected, size_t{1});
        larvae::AssertEqual(storage.Count(), size_t{0});
    });

}
