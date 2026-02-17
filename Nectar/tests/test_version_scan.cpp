#define _CRT_SECURE_NO_WARNINGS
#include <larvae/larvae.h>
#include <nectar/core/asset_blob_header.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/pipeline/asset_importer.h>
#include <nectar/cas/cas_store.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/vfs/memory_mount.h>
#include <nectar/database/asset_database.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <cstdio>
#include <filesystem>

namespace {

    auto& GetVScanAlloc()
    {
        static comb::ModuleAllocator alloc{"TestVersionScan", 8 * 1024 * 1024};
        return alloc.Get();
    }

    nectar::AssetId MakeId(uint64_t v)
    {
        uint8_t bytes[16] = {};
        std::memcpy(bytes, &v, sizeof(v));
        return nectar::AssetId::FromBytes(bytes);
    }

    const char* CasRoot()
    {
#ifdef _WIN32
        return "test_vscan_cas";
#else
        return "/tmp/test_vscan_cas";
#endif
    }

    void CleanupCas()
    {
        std::error_code ec;
        std::filesystem::remove_all(CasRoot(), ec);
    }

    // Configurable-version importer for testing
    class VersionedImporter final : public nectar::AssetImporter<int>
    {
    public:
        uint32_t ver{1};

        wax::Span<const char* const> SourceExtensions() const override
        {
            static const char* const exts[] = {".test"};
            return wax::Span<const char* const>{exts, 1};
        }

        uint32_t Version() const override { return ver; }
        wax::StringView TypeName() const override { return "TestAsset"; }

        nectar::ImportResult Import(wax::ByteSpan source,
                                     const nectar::HiveDocument&,
                                     nectar::ImportContext&) override
        {
            nectar::ImportResult r{};
            r.success = true;
            r.intermediate_data.Append(source.Data(), source.Size());
            return r;
        }
    };

    // Helper to set up full pipeline
    struct PipelineFixture
    {
        comb::DefaultAllocator& alloc;
        nectar::MemoryMountSource mem;
        nectar::VirtualFilesystem vfs;
        nectar::CasStore cas;
        nectar::AssetDatabase db;
        VersionedImporter importer;
        nectar::ImporterRegistry registry;
        nectar::ImportPipeline pipeline;

        PipelineFixture(comb::DefaultAllocator& a)
            : alloc{a}
            , mem{a}
            , vfs{a}
            , cas{a, CasRoot()}
            , db{a}
            , importer{}
            , registry{a}
            , pipeline{a, registry, cas, vfs, db}
        {
            CleanupCas();
            registry.Register(&importer);
            vfs.Mount("", &mem);
        }

        ~PipelineFixture() { CleanupCas(); }

        void AddFile(const char* path, const char* data)
        {
            mem.AddFile(path, wax::ByteSpan{
                reinterpret_cast<const uint8_t*>(data), std::strlen(data)});
        }

        bool Import(uint64_t id, const char* path)
        {
            nectar::ImportRequest req;
            req.source_path = path;
            req.asset_id = MakeId(id);
            return pipeline.ImportAsset(req).success;
        }
    };

    // =========================================================================
    // ScanOutdated
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarVersionScan", "ScanOutdatedEmptyDb", []() {
        auto& alloc = GetVScanAlloc();
        PipelineFixture fix{alloc};

        wax::Vector<nectar::AssetId> out{alloc};
        fix.pipeline.ScanOutdated(out);
        larvae::AssertEqual(out.Size(), size_t{0});
    });

    auto t2 = larvae::RegisterTest("NectarVersionScan", "ScanOutdatedAllCurrent", []() {
        auto& alloc = GetVScanAlloc();
        PipelineFixture fix{alloc};

        fix.AddFile("a.test", "hello");
        fix.AddFile("b.test", "world");
        larvae::AssertTrue(fix.Import(1, "a.test"));
        larvae::AssertTrue(fix.Import(2, "b.test"));

        wax::Vector<nectar::AssetId> out{alloc};
        fix.pipeline.ScanOutdated(out);
        larvae::AssertEqual(out.Size(), size_t{0});
    });

    auto t3 = larvae::RegisterTest("NectarVersionScan", "ScanOutdatedVersionMismatch", []() {
        auto& alloc = GetVScanAlloc();
        PipelineFixture fix{alloc};

        fix.AddFile("a.test", "data1");
        larvae::AssertTrue(fix.Import(1, "a.test"));

        // Bump importer version — asset is now outdated
        fix.importer.ver = 2;

        wax::Vector<nectar::AssetId> out{alloc};
        fix.pipeline.ScanOutdated(out);
        larvae::AssertEqual(out.Size(), size_t{1});
    });

    auto t4 = larvae::RegisterTest("NectarVersionScan", "ScanOutdatedContentChanged", []() {
        auto& alloc = GetVScanAlloc();
        PipelineFixture fix{alloc};

        fix.AddFile("a.test", "original");
        larvae::AssertTrue(fix.Import(1, "a.test"));

        // Change source content — content hash mismatch
        fix.AddFile("a.test", "modified");

        wax::Vector<nectar::AssetId> out{alloc};
        fix.pipeline.ScanOutdated(out);
        larvae::AssertEqual(out.Size(), size_t{1});
    });

    // =========================================================================
    // ReimportOutdated
    // =========================================================================

    auto t5 = larvae::RegisterTest("NectarVersionScan", "ReimportOutdatedSuccess", []() {
        auto& alloc = GetVScanAlloc();
        PipelineFixture fix{alloc};

        fix.AddFile("a.test", "data1");
        fix.AddFile("b.test", "data2");
        larvae::AssertTrue(fix.Import(1, "a.test"));
        larvae::AssertTrue(fix.Import(2, "b.test"));

        // Bump version
        fix.importer.ver = 2;

        wax::Vector<nectar::AssetId> outdated{alloc};
        fix.pipeline.ScanOutdated(outdated);
        larvae::AssertEqual(outdated.Size(), size_t{2});

        size_t count = fix.pipeline.ReimportOutdated(outdated);
        larvae::AssertEqual(count, size_t{2});

        // Should now be current
        wax::Vector<nectar::AssetId> after{alloc};
        fix.pipeline.ScanOutdated(after);
        larvae::AssertEqual(after.Size(), size_t{0});
    });

    auto t6 = larvae::RegisterTest("NectarVersionScan", "ReimportOutdatedPartialFailure", []() {
        auto& alloc = GetVScanAlloc();
        PipelineFixture fix{alloc};

        fix.AddFile("a.test", "data1");
        larvae::AssertTrue(fix.Import(1, "a.test"));

        // Remove source file — reimport will fail
        fix.mem.RemoveFile("a.test");
        fix.importer.ver = 2;

        wax::Vector<nectar::AssetId> outdated{alloc};
        outdated.PushBack(MakeId(1));

        size_t count = fix.pipeline.ReimportOutdated(outdated);
        larvae::AssertEqual(count, size_t{0});
    });

    // =========================================================================
    // AssetBlobHeader
    // =========================================================================

    auto t7 = larvae::RegisterTest("NectarVersionScan", "BlobHeaderWriteRead", []() {
        auto& alloc = GetVScanAlloc();

        const char* payload_str = "test payload data 12345";
        wax::ByteSpan payload{reinterpret_cast<const uint8_t*>(payload_str),
                              std::strlen(payload_str)};

        constexpr uint32_t kMagic = 0x54455354; // "TEST"

        auto blob = nectar::WriteBlob(kMagic, 3, payload, alloc);
        larvae::AssertEqual(blob.Size(), sizeof(nectar::AssetBlobHeader) + payload.Size());

        auto result = nectar::ReadBlob(blob.View(), kMagic);
        larvae::AssertEqual(result.Size(), payload.Size());
        larvae::AssertTrue(std::memcmp(result.Data(), payload_str, payload.Size()) == 0);
    });

    auto t8 = larvae::RegisterTest("NectarVersionScan", "BlobHeaderInvalidMagic", []() {
        auto& alloc = GetVScanAlloc();

        const char* data = "some data";
        wax::ByteSpan payload{reinterpret_cast<const uint8_t*>(data), std::strlen(data)};

        auto blob = nectar::WriteBlob(0xAAAAAAAA, 1, payload, alloc);

        // Wrong magic → empty span
        auto result = nectar::ReadBlob(blob.View(), 0xBBBBBBBB);
        larvae::AssertEqual(result.Size(), size_t{0});
    });

    auto t9 = larvae::RegisterTest("NectarVersionScan", "BlobHeaderTooSmall", []() {
        uint8_t tiny[4] = {1, 2, 3, 4};
        auto result = nectar::ReadBlob(wax::ByteSpan{tiny, 4}, 0x12345678);
        larvae::AssertEqual(result.Size(), size_t{0});
    });

    auto t10 = larvae::RegisterTest("NectarVersionScan", "BlobHeaderCorruptedHash", []() {
        auto& alloc = GetVScanAlloc();

        const char* data = "payload";
        wax::ByteSpan payload{reinterpret_cast<const uint8_t*>(data), std::strlen(data)};

        auto blob = nectar::WriteBlob(0x11111111, 1, payload, alloc);

        // Corrupt one byte in the payload area
        blob.Data()[sizeof(nectar::AssetBlobHeader) + 2] ^= 0xFF;

        auto result = nectar::ReadBlob(blob.View(), 0x11111111);
        larvae::AssertEqual(result.Size(), size_t{0});
    });

} // namespace
