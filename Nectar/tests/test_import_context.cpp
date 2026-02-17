#include <larvae/larvae.h>
#include <nectar/pipeline/import_context.h>
#include <nectar/pipeline/asset_importer.h>
#include <nectar/database/asset_database.h>
#include <nectar/core/asset_id.h>
#include <comb/default_allocator.h>
#include <cstring>

namespace {

    auto& GetImportAlloc()
    {
        static comb::ModuleAllocator alloc{"TestImport", 4 * 1024 * 1024};
        return alloc.Get();
    }

    nectar::AssetId MakeId(uint64_t v)
    {
        uint8_t bytes[16] = {};
        std::memcpy(bytes, &v, sizeof(v));
        return nectar::AssetId::FromBytes(bytes);
    }

    // =========================================================================
    // ImportContext
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarImportContext", "DeclareHardDep", []() {
        auto& alloc = GetImportAlloc();
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(1)};

        ctx.DeclareHardDep(MakeId(2));
        larvae::AssertEqual(ctx.GetDeclaredDeps().Size(), size_t{1});
        larvae::AssertTrue(ctx.GetDeclaredDeps()[0].from == MakeId(1));
        larvae::AssertTrue(ctx.GetDeclaredDeps()[0].to == MakeId(2));
        larvae::AssertTrue(ctx.GetDeclaredDeps()[0].kind == nectar::DepKind::Hard);
    });

    auto t2 = larvae::RegisterTest("NectarImportContext", "DeclareSoftDep", []() {
        auto& alloc = GetImportAlloc();
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(1)};

        ctx.DeclareSoftDep(MakeId(3));
        larvae::AssertTrue(ctx.GetDeclaredDeps()[0].kind == nectar::DepKind::Soft);
    });

    auto t3 = larvae::RegisterTest("NectarImportContext", "DeclareBuildDep", []() {
        auto& alloc = GetImportAlloc();
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(1)};

        ctx.DeclareBuildDep(MakeId(4));
        larvae::AssertTrue(ctx.GetDeclaredDeps()[0].kind == nectar::DepKind::Build);
    });

    auto t4 = larvae::RegisterTest("NectarImportContext", "DeclareInvalidDepIgnored", []() {
        auto& alloc = GetImportAlloc();
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(1)};

        ctx.DeclareHardDep(nectar::AssetId::Invalid());
        larvae::AssertEqual(ctx.GetDeclaredDeps().Size(), size_t{0});
    });

    auto t5 = larvae::RegisterTest("NectarImportContext", "ResolveByPathFound", []() {
        auto& alloc = GetImportAlloc();
        nectar::AssetDatabase db{alloc};

        nectar::AssetRecord r{};
        r.uuid = MakeId(10);
        r.path = wax::String<>{alloc, "textures/hero.png"};
        r.type = wax::String<>{alloc, "Texture"};
        r.name = wax::String<>{alloc, "hero"};
        db.Insert(static_cast<nectar::AssetRecord&&>(r));

        nectar::ImportContext ctx{alloc, db, MakeId(1)};
        auto resolved = ctx.ResolveByPath("textures/hero.png");
        larvae::AssertTrue(resolved.IsValid());
        larvae::AssertTrue(resolved == MakeId(10));
    });

    auto t6 = larvae::RegisterTest("NectarImportContext", "ResolveByPathNotFound", []() {
        auto& alloc = GetImportAlloc();
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(1)};

        auto resolved = ctx.ResolveByPath("nonexistent.png");
        larvae::AssertFalse(resolved.IsValid());
    });

    auto t7 = larvae::RegisterTest("NectarImportContext", "GetCurrentAsset", []() {
        auto& alloc = GetImportAlloc();
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(42)};

        larvae::AssertTrue(ctx.GetCurrentAsset() == MakeId(42));
    });

    // =========================================================================
    // Mock AssetImporter
    // =========================================================================

    struct TestImportAsset { int value; };

    class TestImporter final : public nectar::AssetImporter<TestImportAsset>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".test"};
            return wax::Span<const char* const>{exts, 1};
        }

        uint32_t Version() const override { return 1; }

        wax::StringView TypeName() const override { return "TestImportAsset"; }

        nectar::ImportResult Import(wax::ByteSpan source_data,
                                     const nectar::HiveDocument&,
                                     nectar::ImportContext&) override
        {
            nectar::ImportResult result{};
            if (source_data.Size() < sizeof(int))
            {
                result.error_message = wax::String<>{"Too short"};
                return result;
            }
            result.success = true;
            result.intermediate_data.Append(source_data.Data(), source_data.Size());
            return result;
        }
    };

    auto t8 = larvae::RegisterTest("NectarImportContext", "MockImporterWorks", []() {
        auto& alloc = GetImportAlloc();
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(1)};
        nectar::HiveDocument doc{alloc};

        TestImporter importer;
        int val = 42;
        uint8_t buf[sizeof(int)];
        std::memcpy(buf, &val, sizeof(int));

        auto result = importer.Import(wax::ByteSpan{buf, sizeof(buf)}, doc, ctx);
        larvae::AssertTrue(result.success);
        larvae::AssertEqual(result.intermediate_data.Size(), sizeof(int));
    });

    auto t9 = larvae::RegisterTest("NectarImportContext", "MockImporterExtensions", []() {
        TestImporter importer;
        auto exts = importer.SourceExtensions();
        larvae::AssertEqual(exts.Size(), size_t{1});

        // Compare as C strings
        wax::StringView ext{exts[0]};
        larvae::AssertTrue(ext.Equals(".test"));
    });

}
