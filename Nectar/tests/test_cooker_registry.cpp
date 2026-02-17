#include <larvae/larvae.h>
#include <nectar/pipeline/cooker_registry.h>
#include <nectar/pipeline/asset_cooker.h>
#include <comb/default_allocator.h>
#include <cstring>

namespace {

    auto& GetCookRegAlloc()
    {
        static comb::ModuleAllocator alloc{"TestCookReg", 4 * 1024 * 1024};
        return alloc.Get();
    }

    struct DummyTex {};
    struct DummyMesh {};

    class TextureCooker final : public nectar::AssetCooker<DummyTex>
    {
    public:
        wax::StringView TypeName() const override { return "Texture"; }
        uint32_t Version() const override { return 1; }
        nectar::CookResult Cook(wax::ByteSpan data, const nectar::CookContext& ctx) override
        {
            nectar::CookResult r;
            r.success = true;
            r.cooked_data = wax::ByteBuffer<>{*ctx.alloc};
            r.cooked_data.Resize(data.Size());
            std::memcpy(r.cooked_data.Data(), data.Data(), data.Size());
            return r;
        }
    };

    class MeshCooker final : public nectar::AssetCooker<DummyMesh>
    {
    public:
        wax::StringView TypeName() const override { return "Mesh"; }
        uint32_t Version() const override { return 2; }
        nectar::CookResult Cook(wax::ByteSpan data, const nectar::CookContext& ctx) override
        {
            nectar::CookResult r;
            r.success = true;
            r.cooked_data = wax::ByteBuffer<>{*ctx.alloc};
            r.cooked_data.Resize(data.Size());
            std::memcpy(r.cooked_data.Data(), data.Data(), data.Size());
            return r;
        }
    };

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarCookReg", "RegisterAndFind", []() {
        auto& alloc = GetCookRegAlloc();
        nectar::CookerRegistry reg{alloc};
        TextureCooker tex;
        reg.Register(&tex);

        auto* found = reg.FindByType("Texture");
        larvae::AssertTrue(found != nullptr);
        larvae::AssertTrue(found == &tex);
    });

    auto t2 = larvae::RegisterTest("NectarCookReg", "FindNonExistent", []() {
        auto& alloc = GetCookRegAlloc();
        nectar::CookerRegistry reg{alloc};
        TextureCooker tex;
        reg.Register(&tex);

        larvae::AssertTrue(reg.FindByType("Mesh") == nullptr);
    });

    auto t3 = larvae::RegisterTest("NectarCookReg", "Count", []() {
        auto& alloc = GetCookRegAlloc();
        nectar::CookerRegistry reg{alloc};
        TextureCooker tex;
        MeshCooker mesh;

        larvae::AssertEqual(reg.Count(), size_t{0});
        reg.Register(&tex);
        larvae::AssertEqual(reg.Count(), size_t{1});
        reg.Register(&mesh);
        larvae::AssertEqual(reg.Count(), size_t{2});
    });

    auto t4 = larvae::RegisterTest("NectarCookReg", "MultipleTypes", []() {
        auto& alloc = GetCookRegAlloc();
        nectar::CookerRegistry reg{alloc};
        TextureCooker tex;
        MeshCooker mesh;
        reg.Register(&tex);
        reg.Register(&mesh);

        larvae::AssertTrue(reg.FindByType("Texture") == &tex);
        larvae::AssertTrue(reg.FindByType("Mesh") == &mesh);
    });

    auto t5 = larvae::RegisterTest("NectarCookReg", "OverwriteLastWins", []() {
        auto& alloc = GetCookRegAlloc();
        nectar::CookerRegistry reg{alloc};
        TextureCooker tex1;
        TextureCooker tex2;
        reg.Register(&tex1);
        reg.Register(&tex2);

        larvae::AssertTrue(reg.FindByType("Texture") == &tex2);
    });

    auto t6 = larvae::RegisterTest("NectarCookReg", "NullCooker", []() {
        auto& alloc = GetCookRegAlloc();
        nectar::CookerRegistry reg{alloc};
        reg.Register(nullptr);
        larvae::AssertEqual(reg.Count(), size_t{0});
    });

    auto t7 = larvae::RegisterTest("NectarCookReg", "EmptyRegistry", []() {
        auto& alloc = GetCookRegAlloc();
        nectar::CookerRegistry reg{alloc};
        larvae::AssertTrue(reg.FindByType("Texture") == nullptr);
        larvae::AssertEqual(reg.Count(), size_t{0});
    });

    auto t8 = larvae::RegisterTest("NectarCookReg", "CookerVersion", []() {
        auto& alloc = GetCookRegAlloc();
        nectar::CookerRegistry reg{alloc};
        TextureCooker tex;
        MeshCooker mesh;
        reg.Register(&tex);
        reg.Register(&mesh);

        auto* t = reg.FindByType("Texture");
        auto* m = reg.FindByType("Mesh");
        larvae::AssertEqual(t->Version(), uint32_t{1});
        larvae::AssertEqual(m->Version(), uint32_t{2});
    });

}
