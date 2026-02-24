#include <larvae/larvae.h>
#include <nectar/pipeline/hot_reload.h>
#include <nectar/pipeline/cook_pipeline.h>
#include <nectar/pipeline/cooker_registry.h>
#include <nectar/pipeline/cook_cache.h>
#include <nectar/pipeline/asset_cooker.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/pipeline/asset_importer.h>
#include <nectar/cas/cas_store.h>
#include <nectar/database/asset_database.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/vfs/memory_mount.h>
#include <nectar/watcher/file_watcher.h>
#include <nectar/hive/hive_document.h>
#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <filesystem>

namespace {

    auto& GetHrAlloc()
    {
        static comb::ModuleAllocator alloc{"TestHotReload", 8 * 1024 * 1024};
        return alloc.Get();
    }

    struct TempDir
    {
        std::filesystem::path path;
        explicit TempDir(const char* name)
        {
            path = std::filesystem::temp_directory_path() / name;
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
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

    // Mock file watcher: injects changes on demand
    class MockFileWatcher final : public nectar::IFileWatcher
    {
    public:
        explicit MockFileWatcher(comb::DefaultAllocator& alloc) : alloc_{&alloc}, pending_{alloc} {}

        void Watch(wax::StringView) override {}

        void Poll(wax::Vector<nectar::FileChange>& changes) override
        {
            for (size_t i = 0; i < pending_.Size(); ++i)
            {
                nectar::FileChange c;
                c.path = wax::String<>{*alloc_};
                c.path.Append(pending_[i].path.CStr(), pending_[i].path.Size());
                c.kind = pending_[i].kind;
                changes.PushBack(static_cast<nectar::FileChange&&>(c));
            }
            pending_.Clear();
        }

        void Inject(wax::StringView path, nectar::FileChangeKind kind)
        {
            nectar::FileChange c;
            c.path = wax::String<>{*alloc_};
            c.path.Append(path.Data(), path.Size());
            c.kind = kind;
            pending_.PushBack(static_cast<nectar::FileChange&&>(c));
        }

    private:
        comb::DefaultAllocator* alloc_;
        wax::Vector<nectar::FileChange> pending_;
    };

    // Passthrough importer for tests
    struct DummyAssetHr {};

    class TestImporter final : public nectar::AssetImporter<DummyAssetHr>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".dat"};
            return {exts, 1};
        }
        uint32_t Version() const override { return 1; }
        wax::StringView TypeName() const override { return "TestAsset"; }
        nectar::ImportResult Import(wax::ByteSpan source_data,
                                     const nectar::HiveDocument&,
                                     nectar::ImportContext&) override
        {
            nectar::ImportResult r{};
            r.success = true;
            r.intermediate_data = wax::ByteBuffer<>{GetHrAlloc()};
            r.intermediate_data.Append(source_data.Data(), source_data.Size());
            return r;
        }
    };

    // Importer that captures settings for verification
    class SettingsCapturingImporter final : public nectar::AssetImporter<DummyAssetHr>
    {
    public:
        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".mesh"};
            return {exts, 1};
        }
        uint32_t Version() const override { return 1; }
        wax::StringView TypeName() const override { return "MeshAsset"; }
        nectar::ImportResult Import(wax::ByteSpan source_data,
                                     const nectar::HiveDocument& settings,
                                     nectar::ImportContext&) override
        {
            // Capture base_path setting
            last_base_path = std::string{settings.GetString("import", "base_path").Data(),
                                          settings.GetString("import", "base_path").Size()};
            nectar::ImportResult r{};
            r.success = true;
            r.intermediate_data = wax::ByteBuffer<>{GetHrAlloc()};
            r.intermediate_data.Append(source_data.Data(), source_data.Size());
            return r;
        }
        std::string last_base_path;
    };

    // Passthrough cooker for tests
    struct DummyCookedHr {};

    class TestCooker final : public nectar::AssetCooker<DummyCookedHr>
    {
    public:
        explicit TestCooker(wax::StringView type_name) : type_{type_name.Data(), type_name.Size()} {}
        wax::StringView TypeName() const override { return wax::StringView{type_.c_str()}; }
        uint32_t Version() const override { return 1; }
        nectar::CookResult Cook(wax::ByteSpan data, const nectar::CookContext& ctx) override
        {
            nectar::CookResult r;
            r.success = true;
            r.cooked_data = wax::ByteBuffer<>{*ctx.alloc};
            r.cooked_data.Append(data.Data(), data.Size());
            return r;
        }
    private:
        std::string type_;
    };

    // Pre-populate an asset record in the DB
    void SetupRecord(nectar::AssetDatabase& db, nectar::CasStore& cas,
                      nectar::AssetId id, const char* path, const char* type,
                      const void* data, size_t size)
    {
        auto& alloc = GetHrAlloc();
        wax::ByteSpan span{data, size};
        nectar::ContentHash cas_hash = cas.Store(span);

        nectar::AssetRecord record{};
        record.uuid = id;
        record.path = wax::String<>{alloc, path};
        record.type = wax::String<>{alloc, type};
        record.name = wax::String<>{alloc};
        record.content_hash = nectar::ContentHash::FromData(data, size);
        record.intermediate_hash = cas_hash;
        record.import_version = 1;
        record.labels = wax::Vector<wax::String<>>{alloc};
        db.Insert(static_cast<nectar::AssetRecord&&>(record));
    }

    // Infra: all the pipelines wired together
    struct TestEnv
    {
        comb::DefaultAllocator& alloc;
        TempDir cas_dir;
        nectar::MemoryMountSource mem;
        nectar::VirtualFilesystem vfs;
        nectar::AssetDatabase db;
        nectar::CasStore cas;
        nectar::ImporterRegistry import_registry;
        nectar::ImportPipeline import_pipeline;
        nectar::CookerRegistry cook_registry;
        nectar::CookCache cook_cache;
        nectar::CookPipeline cook_pipeline;
        MockFileWatcher watcher;

        explicit TestEnv(const char* cas_name)
            : alloc{GetHrAlloc()}
            , cas_dir{cas_name}
            , mem{alloc}
            , vfs{alloc}
            , db{alloc}
            , cas{alloc, cas_dir.View()}
            , import_registry{alloc}
            , import_pipeline{alloc, import_registry, cas, vfs, db}
            , cook_registry{alloc}
            , cook_cache{alloc}
            , cook_pipeline{alloc, cook_registry, cas, db, cook_cache}
            , watcher{alloc}
        {
            vfs.Mount("", &mem);
        }
    };
}

// ============================================================================
// Tests
// ============================================================================

auto t1 = larvae::RegisterTest("NectarHotReload", "ProcessChangesEmpty", []() {
    TestEnv env{"hr_test_1"};
    TestImporter importer;
    env.import_registry.Register(&importer);
    TestCooker cooker{"TestAsset"};
    env.cook_registry.Register(&cooker);

    nectar::HotReloadManager mgr{env.alloc, env.watcher, env.db, env.import_pipeline, env.cook_pipeline};

    // No changes injected
    size_t count = mgr.ProcessChanges("pc");
    larvae::AssertEqual(count, size_t{0});
    larvae::AssertEqual(mgr.LastReloaded().Size(), size_t{0});
});

auto t2 = larvae::RegisterTest("NectarHotReload", "DeletedIgnored", []() {
    TestEnv env{"hr_test_2"};
    TestImporter importer;
    env.import_registry.Register(&importer);
    TestCooker cooker{"TestAsset"};
    env.cook_registry.Register(&cooker);

    const char* data = "abc";
    auto id = MakeId(10);
    env.mem.AddFile("data/test.dat", wax::ByteSpan{data, 3});
    SetupRecord(env.db, env.cas, id, "data/test.dat", "TestAsset", data, 3);

    nectar::HotReloadManager mgr{env.alloc, env.watcher, env.db, env.import_pipeline, env.cook_pipeline};

    // Deleted event should be ignored
    env.watcher.Inject("data/test.dat", nectar::FileChangeKind::Deleted);
    size_t count = mgr.ProcessChanges("pc");
    larvae::AssertEqual(count, size_t{0});
});

auto t3 = larvae::RegisterTest("NectarHotReload", "UnknownPathIgnored", []() {
    TestEnv env{"hr_test_3"};
    TestImporter importer;
    env.import_registry.Register(&importer);
    TestCooker cooker{"TestAsset"};
    env.cook_registry.Register(&cooker);

    nectar::HotReloadManager mgr{env.alloc, env.watcher, env.db, env.import_pipeline, env.cook_pipeline};

    // Path not in DB
    env.watcher.Inject("data/unknown.dat", nectar::FileChangeKind::Modified);
    size_t count = mgr.ProcessChanges("pc");
    larvae::AssertEqual(count, size_t{0});
});

auto t4 = larvae::RegisterTest("NectarHotReload", "ReimportAndRecook", []() {
    TestEnv env{"hr_test_4"};
    TestImporter importer;
    env.import_registry.Register(&importer);
    TestCooker cooker{"TestAsset"};
    env.cook_registry.Register(&cooker);

    const char* data = "hello";
    auto id = MakeId(20);
    env.mem.AddFile("data/test.dat", wax::ByteSpan{data, 5});
    SetupRecord(env.db, env.cas, id, "data/test.dat", "TestAsset", data, 5);

    nectar::HotReloadManager mgr{env.alloc, env.watcher, env.db, env.import_pipeline, env.cook_pipeline};

    // Modify the file content
    const char* new_data = "world";
    env.mem.AddFile("data/test.dat", wax::ByteSpan{new_data, 5});

    env.watcher.Inject("data/test.dat", nectar::FileChangeKind::Modified);
    size_t count = mgr.ProcessChanges("pc");

    larvae::AssertEqual(count, size_t{1});
    larvae::AssertEqual(mgr.LastReloaded().Size(), size_t{1});
    larvae::AssertTrue(mgr.LastReloaded()[0] == id);

    // Cook cache should have an entry
    auto* cook_entry = env.cook_cache.Find(id, "pc");
    larvae::AssertTrue(cook_entry != nullptr);
    larvae::AssertTrue(cook_entry->cooked_hash.IsValid());

    // Cooked blob should be loadable from CAS
    auto blob = env.cas.Load(cook_entry->cooked_hash);
    larvae::AssertTrue(blob.Size() > 0);
});

auto t5 = larvae::RegisterTest("NectarHotReload", "BaseDirectoryStripsPrefix", []() {
    TestEnv env{"hr_test_5"};
    TestImporter importer;
    env.import_registry.Register(&importer);
    TestCooker cooker{"TestAsset"};
    env.cook_registry.Register(&cooker);

    const char* data = "xyz";
    auto id = MakeId(30);
    env.mem.AddFile("data/test.dat", wax::ByteSpan{data, 3});
    SetupRecord(env.db, env.cas, id, "data/test.dat", "TestAsset", data, 3);

    nectar::HotReloadManager mgr{env.alloc, env.watcher, env.db, env.import_pipeline, env.cook_pipeline};
    mgr.SetBaseDirectory("/base/dir");

    // Inject absolute path — should strip "/base/dir/" prefix
    env.watcher.Inject("/base/dir/data/test.dat", nectar::FileChangeKind::Modified);
    size_t count = mgr.ProcessChanges("pc");

    larvae::AssertEqual(count, size_t{1});
    larvae::AssertTrue(mgr.LastReloaded()[0] == id);
});

auto t6 = larvae::RegisterTest("NectarHotReload", "BaseDirectoryBackslashNormalize", []() {
    TestEnv env{"hr_test_6"};
    TestImporter importer;
    env.import_registry.Register(&importer);
    TestCooker cooker{"TestAsset"};
    env.cook_registry.Register(&cooker);

    const char* data = "abc";
    auto id = MakeId(40);
    env.mem.AddFile("data/test.dat", wax::ByteSpan{data, 3});
    SetupRecord(env.db, env.cas, id, "data/test.dat", "TestAsset", data, 3);

    nectar::HotReloadManager mgr{env.alloc, env.watcher, env.db, env.import_pipeline, env.cook_pipeline};
    // Windows-style path with backslashes
    mgr.SetBaseDirectory("C:\\Users\\test\\assets");

    env.watcher.Inject("C:/Users/test/assets/data/test.dat", nectar::FileChangeKind::Modified);
    size_t count = mgr.ProcessChanges("pc");

    larvae::AssertEqual(count, size_t{1});
});

auto t7 = larvae::RegisterTest("NectarHotReload", "SettingsProviderCalled", []() {
    TestEnv env{"hr_test_7"};
    SettingsCapturingImporter mesh_importer;
    env.import_registry.Register(&mesh_importer);
    TestCooker cooker{"MeshAsset"};
    env.cook_registry.Register(&cooker);

    const char* data = "mesh_data";
    auto id = MakeId(50);
    env.mem.AddFile("scene/model.mesh", wax::ByteSpan{data, 9});
    SetupRecord(env.db, env.cas, id, "scene/model.mesh", "MeshAsset", data, 9);

    nectar::HotReloadManager mgr{env.alloc, env.watcher, env.db, env.import_pipeline, env.cook_pipeline};

    // Settings provider that sets base_path
    static bool provider_called = false;
    provider_called = false;
    mgr.SetImportSettingsProvider([](nectar::AssetId, wax::StringView,
                                     nectar::HiveDocument& settings, void*) {
        provider_called = true;
        settings.SetValue("import", "base_path",
            nectar::HiveValue::MakeString(settings.GetAllocator(), "/some/path/model.mesh"));
    }, nullptr);

    env.watcher.Inject("scene/model.mesh", nectar::FileChangeKind::Modified);
    size_t count = mgr.ProcessChanges("pc");

    larvae::AssertEqual(count, size_t{1});
    larvae::AssertTrue(provider_called);
    larvae::AssertEqual(mesh_importer.last_base_path, std::string{"/some/path/model.mesh"});
});
