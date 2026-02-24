#include <larvae/larvae.h>
#include <nectar/server/asset_loader.h>
#include <wax/serialization/byte_span.h>
#include <comb/default_allocator.h>
#include <comb/new.h>
#include <cstring>

namespace {

    struct TestAsset
    {
        int value;
        float data;
    };

    class TestAssetLoader final : public nectar::AssetLoader<TestAsset>
    {
    public:
        TestAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            if (data.Size() < sizeof(int) + sizeof(float)) return nullptr;

            auto* asset = comb::New<TestAsset>(alloc);
            asset->value = data.Read<int>(0);
            asset->data = data.Read<float>(sizeof(int));
            return asset;
        }

        void Unload(TestAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset) comb::Delete(alloc, asset);
        }
    };

    auto& GetTestAllocator()
    {
        static comb::ModuleAllocator alloc{"TestLoader", 1 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // Tests
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarAssetLoader", "LoadValidData", []() {
        TestAssetLoader loader;
        auto& alloc = GetTestAllocator();

        int v = 42;
        float f = 3.14f;
        uint8_t buf[sizeof(int) + sizeof(float)];
        std::memcpy(buf, &v, sizeof(int));
        std::memcpy(buf + sizeof(int), &f, sizeof(float));

        wax::ByteSpan span{buf, sizeof(buf)};
        TestAsset* asset = loader.Load(span, alloc);

        larvae::AssertNotNull(asset);
        larvae::AssertEqual(asset->value, 42);
        larvae::AssertFloatEqual(asset->data, 3.14f);

        loader.Unload(asset, alloc);
    });

    auto t2 = larvae::RegisterTest("NectarAssetLoader", "LoadInsufficientData", []() {
        TestAssetLoader loader;
        auto& alloc = GetTestAllocator();

        uint8_t buf[2] = {0, 0};
        wax::ByteSpan span{buf, 2};
        TestAsset* asset = loader.Load(span, alloc);
        larvae::AssertNull(asset);
    });

    auto t3 = larvae::RegisterTest("NectarAssetLoader", "LoadEmptyData", []() {
        TestAssetLoader loader;
        auto& alloc = GetTestAllocator();

        wax::ByteSpan span{};
        TestAsset* asset = loader.Load(span, alloc);
        larvae::AssertNull(asset);
    });

    auto t4 = larvae::RegisterTest("NectarAssetLoader", "UnloadNull", []() {
        TestAssetLoader loader;
        auto& alloc = GetTestAllocator();
        loader.Unload(nullptr, alloc);
        // Should not crash
        larvae::AssertTrue(true);
    });

    auto t5 = larvae::RegisterTest("NectarAssetLoader", "LoadedValuesCorrect", []() {
        TestAssetLoader loader;
        auto& alloc = GetTestAllocator();

        int v = -999;
        float f = 0.001f;
        uint8_t buf[sizeof(int) + sizeof(float)];
        std::memcpy(buf, &v, sizeof(int));
        std::memcpy(buf + sizeof(int), &f, sizeof(float));

        wax::ByteSpan span{buf, sizeof(buf)};
        TestAsset* asset = loader.Load(span, alloc);
        larvae::AssertNotNull(asset);
        larvae::AssertEqual(asset->value, -999);
        larvae::AssertFloatEqual(asset->data, 0.001f);

        loader.Unload(asset, alloc);
    });

    auto t6 = larvae::RegisterTest("NectarAssetLoader", "ExactMinimumSize", []() {
        TestAssetLoader loader;
        auto& alloc = GetTestAllocator();

        // Exactly sizeof(int) + sizeof(float) bytes
        uint8_t buf[sizeof(int) + sizeof(float)]{};
        wax::ByteSpan span{buf, sizeof(buf)};
        TestAsset* asset = loader.Load(span, alloc);
        larvae::AssertNotNull(asset);

        loader.Unload(asset, alloc);
    });

}
