#include <larvae/larvae.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/pipeline/asset_importer.h>
#include <nectar/cas/cas_store.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/vfs/memory_mount.h>
#include <nectar/database/asset_database.h>
#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <nectar/hive/hive_document.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <filesystem>

namespace {

    auto& GetPipeAlloc()
    {
        static comb::ModuleAllocator alloc{"TestPipeline", 4 * 1024 * 1024};
        return alloc.Get();
    }

    struct TempDir
    {
        std::filesystem::path path;
        explicit TempDir(const char* name)
        {
            path = std::filesystem::temp_directory_path() / name;
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

    // Simple pass-through importer
    struct DummyAsset {};

    class PassthroughImporter final : public nectar::AssetImporter<DummyAsset>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".dat"};
            return {exts, 1};
        }
        uint32_t Version() const override { return version_; }
        wax::StringView TypeName() const override { return "DummyAsset"; }
        nectar::ImportResult Import(wax::ByteSpan source_data,
                                     const nectar::HiveDocument&,
                                     nectar::ImportContext&) override
        {
            nectar::ImportResult r{};
            r.success = true;
            r.intermediate_data = wax::ByteBuffer<>{GetPipeAlloc()};
            r.intermediate_data.Append(source_data.Data(), source_data.Size());
            return r;
        }

        uint32_t version_{1};
    };

    // Importer that always fails
    class FailingImporter final : public nectar::AssetImporter<DummyAsset>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".fail"};
            return {exts, 1};
        }
        uint32_t Version() const override { return 1; }
        wax::StringView TypeName() const override { return "FailAsset"; }
        nectar::ImportResult Import(wax::ByteSpan, const nectar::HiveDocument&,
                                     nectar::ImportContext&) override
        {
            nectar::ImportResult r{};
            r.error_message = wax::String<>{GetPipeAlloc(), "import failed on purpose"};
            return r;
        }
    };

    // Importer that declares a dependency
    class DepImporter final : public nectar::AssetImporter<DummyAsset>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".dep"};
            return {exts, 1};
        }
        uint32_t Version() const override { return 1; }
        wax::StringView TypeName() const override { return "DepAsset"; }

        nectar::AssetId dep_target;

        nectar::ImportResult Import(wax::ByteSpan source_data,
                                     const nectar::HiveDocument&,
                                     nectar::ImportContext& ctx) override
        {
            ctx.DeclareHardDep(dep_target);
            nectar::ImportResult r{};
            r.success = true;
            r.intermediate_data = wax::ByteBuffer<>{GetPipeAlloc()};
            r.intermediate_data.Append(source_data.Data(), source_data.Size());
            return r;
        }
    };

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarPipeline", "ImportSuccess", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_1"};

        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("data/test.dat", wax::ByteSpan{"hello", 5});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};
        PassthroughImporter importer;
        registry.Register(&importer);

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        auto id = MakeId(100);
        nectar::ImportRequest req{"data/test.dat", id};
        auto output = pipeline.ImportAsset(req);

        larvae::AssertTrue(output.success);
        larvae::AssertTrue(output.content_hash.IsValid());
        larvae::AssertEqual(output.import_version, uint32_t{1});
    });

    auto t2 = larvae::RegisterTest("NectarPipeline", "ImportNoImporter", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_2"};

        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("test.xyz", wax::ByteSpan{"data", 4});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};  // nothing registered

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        nectar::ImportRequest req{"test.xyz", MakeId(200)};
        auto output = pipeline.ImportAsset(req);

        larvae::AssertFalse(output.success);
        larvae::AssertTrue(output.error_message.View().Size() > 0);
    });

    auto t3 = larvae::RegisterTest("NectarPipeline", "ImportSourceNotFound", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_3"};

        nectar::MemoryMountSource mem{alloc};
        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};
        PassthroughImporter importer;
        registry.Register(&importer);

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        nectar::ImportRequest req{"missing.dat", MakeId(300)};
        auto output = pipeline.ImportAsset(req);

        larvae::AssertFalse(output.success);
    });

    auto t4 = larvae::RegisterTest("NectarPipeline", "ImportFailure", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_4"};

        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("bad.fail", wax::ByteSpan{"data", 4});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};
        FailingImporter fail_importer;
        registry.Register(&fail_importer);

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        nectar::ImportRequest req{"bad.fail", MakeId(400)};
        auto output = pipeline.ImportAsset(req);

        larvae::AssertFalse(output.success);
        larvae::AssertTrue(output.error_message.View().Size() > 0);
    });

    auto t5 = larvae::RegisterTest("NectarPipeline", "StoreInCas", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_5"};

        nectar::MemoryMountSource mem{alloc};
        const char* content = "cas blob content";
        mem.AddFile("blob.dat", wax::ByteSpan{content, std::strlen(content)});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};
        PassthroughImporter importer;
        registry.Register(&importer);

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        nectar::ImportRequest req{"blob.dat", MakeId(500)};
        auto output = pipeline.ImportAsset(req);
        larvae::AssertTrue(output.success);

        // Verify the blob is in the CAS
        larvae::AssertTrue(cas.Contains(output.content_hash));

        auto loaded = cas.Load(output.content_hash);
        larvae::AssertEqual(loaded.Size(), std::strlen(content));
        larvae::AssertTrue(std::memcmp(loaded.Data(), content, loaded.Size()) == 0);
    });

    auto t6 = larvae::RegisterTest("NectarPipeline", "UpdateDatabase", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_6"};

        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("rec.dat", wax::ByteSpan{"record", 6});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};
        PassthroughImporter importer;
        registry.Register(&importer);

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        auto id = MakeId(600);
        nectar::ImportRequest req{"rec.dat", id};
        auto output = pipeline.ImportAsset(req);
        larvae::AssertTrue(output.success);

        // Record should be in the database
        auto* record = db.FindByUuid(id);
        larvae::AssertTrue(record != nullptr);
        // record.content_hash = source hash (for change detection)
        auto source_hash = nectar::ContentHash::FromData("record", 6);
        larvae::AssertTrue(record->content_hash == source_hash);
        larvae::AssertEqual(record->import_version, uint32_t{1});
        larvae::AssertTrue(record->type.View().Equals("DummyAsset"));
    });

    auto t7 = larvae::RegisterTest("NectarPipeline", "DependenciesRecorded", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_7"};

        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("a.dep", wax::ByteSpan{"dep", 3});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};
        DepImporter dep_importer;
        dep_importer.dep_target = MakeId(999);
        registry.Register(&dep_importer);

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        auto id = MakeId(700);
        nectar::ImportRequest req{"a.dep", id};
        auto output = pipeline.ImportAsset(req);
        larvae::AssertTrue(output.success);

        // Should have one dependency
        larvae::AssertEqual(output.dependencies.Size(), size_t{1});
        larvae::AssertTrue(output.dependencies[0].from == id);
        larvae::AssertTrue(output.dependencies[0].to == MakeId(999));
        larvae::AssertTrue(output.dependencies[0].kind == nectar::DepKind::Hard);

        // Graph should also have it
        auto& graph = db.GetDependencyGraph();
        larvae::AssertTrue(graph.HasEdge(id, MakeId(999)));
    });

    auto t8 = larvae::RegisterTest("NectarPipeline", "NeedsReimportNewAsset", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_8"};

        nectar::MemoryMountSource mem{alloc};
        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        // Unknown asset -> needs reimport
        larvae::AssertTrue(pipeline.NeedsReimport(MakeId(800)));
    });

    auto t9 = larvae::RegisterTest("NectarPipeline", "NeedsReimportVersionChanged", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_9"};

        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("v.dat", wax::ByteSpan{"ver", 3});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};
        PassthroughImporter importer;
        registry.Register(&importer);

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        auto id = MakeId(900);
        nectar::ImportRequest req{"v.dat", id};
        (void)pipeline.ImportAsset(req);

        // Same version: no reimport needed
        larvae::AssertFalse(pipeline.NeedsReimport(id));

        // Bump version
        importer.version_ = 2;
        larvae::AssertTrue(pipeline.NeedsReimport(id));
    });

    auto t10 = larvae::RegisterTest("NectarPipeline", "NeedsReimportContentChanged", []() {
        auto& alloc = GetPipeAlloc();
        TempDir cas_dir{"nectar_pipe_test_10"};

        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("c.dat", wax::ByteSpan{"original", 8});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::AssetDatabase db{alloc};
        nectar::CasStore cas{alloc, cas_dir.View()};
        nectar::ImporterRegistry registry{alloc};
        PassthroughImporter importer;
        registry.Register(&importer);

        nectar::ImportPipeline pipeline{alloc, registry, cas, vfs, db};

        auto id = MakeId(1000);
        nectar::ImportRequest req{"c.dat", id};
        (void)pipeline.ImportAsset(req);

        // Same content: no reimport
        larvae::AssertFalse(pipeline.NeedsReimport(id));

        // Change the source file
        mem.AddFile("c.dat", wax::ByteSpan{"modified", 8});
        larvae::AssertTrue(pipeline.NeedsReimport(id));
    });

}
