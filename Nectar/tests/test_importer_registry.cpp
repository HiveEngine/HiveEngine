#include <larvae/larvae.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/pipeline/asset_importer.h>
#include <comb/default_allocator.h>

namespace {

    auto& GetRegAlloc()
    {
        static comb::ModuleAllocator alloc{"TestRegistry", 4 * 1024 * 1024};
        return alloc.Get();
    }

    struct DummyAsset {};

    class PngImporter final : public nectar::AssetImporter<DummyAsset>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".png"};
            return {exts, 1};
        }
        uint32_t Version() const override { return 1; }
        wax::StringView TypeName() const override { return "Texture"; }
        nectar::ImportResult Import(wax::ByteSpan, const nectar::HiveDocument&,
                                     nectar::ImportContext&) override
        {
            return {};
        }
    };

    class JpgImporter final : public nectar::AssetImporter<DummyAsset>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".jpg", ".jpeg"};
            return {exts, 2};
        }
        uint32_t Version() const override { return 2; }
        wax::StringView TypeName() const override { return "Texture"; }
        nectar::ImportResult Import(wax::ByteSpan, const nectar::HiveDocument&,
                                     nectar::ImportContext&) override
        {
            return {};
        }
    };

    class MeshImporter final : public nectar::AssetImporter<DummyAsset>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".glb", ".gltf"};
            return {exts, 2};
        }
        uint32_t Version() const override { return 1; }
        wax::StringView TypeName() const override { return "Mesh"; }
        nectar::ImportResult Import(wax::ByteSpan, const nectar::HiveDocument&,
                                     nectar::ImportContext&) override
        {
            return {};
        }
    };

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarRegistry", "RegisterAndFind", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};
        PngImporter png;
        reg.Register(&png);

        auto* found = reg.FindByExtension(".png");
        larvae::AssertTrue(found != nullptr);
        larvae::AssertTrue(found == &png);
    });

    auto t2 = larvae::RegisterTest("NectarRegistry", "FindByPath", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};
        PngImporter png;
        reg.Register(&png);

        auto* found = reg.FindByPath("textures/hero.png");
        larvae::AssertTrue(found != nullptr);
        larvae::AssertTrue(found == &png);
    });

    auto t3 = larvae::RegisterTest("NectarRegistry", "FindNonExistent", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};
        PngImporter png;
        reg.Register(&png);

        larvae::AssertTrue(reg.FindByExtension(".bmp") == nullptr);
    });

    auto t4 = larvae::RegisterTest("NectarRegistry", "MultipleExtensions", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};
        JpgImporter jpg;
        reg.Register(&jpg);

        larvae::AssertTrue(reg.FindByExtension(".jpg") == &jpg);
        larvae::AssertTrue(reg.FindByExtension(".jpeg") == &jpg);
    });

    auto t5 = larvae::RegisterTest("NectarRegistry", "Count", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};
        PngImporter png;
        JpgImporter jpg;

        larvae::AssertEqual(reg.Count(), size_t{0});
        reg.Register(&png);
        larvae::AssertEqual(reg.Count(), size_t{1});
        reg.Register(&jpg);
        larvae::AssertEqual(reg.Count(), size_t{3});  // .png + .jpg + .jpeg
    });

    auto t6 = larvae::RegisterTest("NectarRegistry", "EmptyPath", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};
        PngImporter png;
        reg.Register(&png);

        larvae::AssertTrue(reg.FindByPath("") == nullptr);
    });

    auto t7 = larvae::RegisterTest("NectarRegistry", "CaseInsensitive", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};
        PngImporter png;
        reg.Register(&png);

        larvae::AssertTrue(reg.FindByExtension(".PNG") != nullptr);
        larvae::AssertTrue(reg.FindByPath("textures/Hero.PNG") != nullptr);
    });

    auto t8 = larvae::RegisterTest("NectarRegistry", "OverwriteLastWins", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};

        // Both claim ".png"
        PngImporter png1;
        PngImporter png2;

        // Hack: register a second importer that also claims .png
        // We'll use MeshImporter but that claims .glb/.gltf, so let's just
        // register png1 then png2 — both are PngImporter with .png ext
        reg.Register(&png1);
        reg.Register(&png2);

        // Last registration wins
        auto* found = reg.FindByExtension(".png");
        larvae::AssertTrue(found == &png2);
    });

    auto t9 = larvae::RegisterTest("NectarRegistry", "MultipleImporters", []() {
        auto& alloc = GetRegAlloc();
        nectar::ImporterRegistry reg{alloc};

        PngImporter png;
        JpgImporter jpg;
        MeshImporter mesh;
        reg.Register(&png);
        reg.Register(&jpg);
        reg.Register(&mesh);

        larvae::AssertTrue(reg.FindByPath("hero.png") == &png);
        larvae::AssertTrue(reg.FindByPath("hero.jpg") == &jpg);
        larvae::AssertTrue(reg.FindByPath("hero.jpeg") == &jpg);
        larvae::AssertTrue(reg.FindByPath("sword.glb") == &mesh);
        larvae::AssertTrue(reg.FindByPath("sword.gltf") == &mesh);
        larvae::AssertTrue(reg.FindByPath("data.bin") == nullptr);
    });

}
