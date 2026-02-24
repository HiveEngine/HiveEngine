#include <larvae/larvae.h>
#include <nectar/cas/cas_store.h>
#include <nectar/core/content_hash.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <filesystem>

namespace {

    auto& GetCasAlloc()
    {
        static comb::ModuleAllocator alloc{"TestCas", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // Creates a temp directory and cleans it up on destruction
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
            // store the string so it lives long enough
            static thread_local std::string s;
            s = path.string();
            return wax::StringView{s.c_str()};
        }
    };

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarCAS", "StoreAndLoad", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_1"};
        nectar::CasStore cas{alloc, dir.View()};

        const char* data = "hello cas store";
        auto hash = cas.Store(wax::ByteSpan{data, std::strlen(data)});
        larvae::AssertTrue(hash.IsValid());

        auto loaded = cas.Load(hash);
        larvae::AssertEqual(loaded.Size(), std::strlen(data));
        larvae::AssertTrue(std::memcmp(loaded.Data(), data, std::strlen(data)) == 0);
    });

    auto t2 = larvae::RegisterTest("NectarCAS", "StoreEmpty", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_2"};
        nectar::CasStore cas{alloc, dir.View()};

        auto hash = cas.Store(wax::ByteSpan{static_cast<const uint8_t*>(nullptr), size_t{0}});
        larvae::AssertTrue(hash.IsValid());

        auto loaded = cas.Load(hash);
        larvae::AssertEqual(loaded.Size(), size_t{0});
    });

    auto t3 = larvae::RegisterTest("NectarCAS", "StoreDuplicate", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_3"};
        nectar::CasStore cas{alloc, dir.View()};

        const char* data = "dedup test";
        auto hash1 = cas.Store(wax::ByteSpan{data, std::strlen(data)});
        auto hash2 = cas.Store(wax::ByteSpan{data, std::strlen(data)});

        larvae::AssertTrue(hash1 == hash2);

        auto loaded = cas.Load(hash1);
        larvae::AssertEqual(loaded.Size(), std::strlen(data));
    });

    auto t4 = larvae::RegisterTest("NectarCAS", "Contains", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_4"};
        nectar::CasStore cas{alloc, dir.View()};

        const char* data = "exists?";
        auto hash = cas.Store(wax::ByteSpan{data, std::strlen(data)});

        larvae::AssertTrue(cas.Contains(hash));
    });

    auto t5 = larvae::RegisterTest("NectarCAS", "ContainsMissing", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_5"};
        nectar::CasStore cas{alloc, dir.View()};

        nectar::ContentHash fake{0x1234, 0x5678};
        larvae::AssertFalse(cas.Contains(fake));
    });

    auto t6 = larvae::RegisterTest("NectarCAS", "LoadMissing", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_6"};
        nectar::CasStore cas{alloc, dir.View()};

        nectar::ContentHash fake{0xAAAA, 0xBBBB};
        auto loaded = cas.Load(fake);
        larvae::AssertEqual(loaded.Size(), size_t{0});
    });

    auto t7 = larvae::RegisterTest("NectarCAS", "Remove", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_7"};
        nectar::CasStore cas{alloc, dir.View()};

        const char* data = "remove me";
        auto hash = cas.Store(wax::ByteSpan{data, std::strlen(data)});
        larvae::AssertTrue(cas.Contains(hash));

        bool removed = cas.Remove(hash);
        larvae::AssertTrue(removed);
        larvae::AssertFalse(cas.Contains(hash));
    });

    auto t8 = larvae::RegisterTest("NectarCAS", "RemoveMissing", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_8"};
        nectar::CasStore cas{alloc, dir.View()};

        nectar::ContentHash fake{0xDEAD, 0xBEEF};
        bool removed = cas.Remove(fake);
        larvae::AssertFalse(removed);
    });

    auto t9 = larvae::RegisterTest("NectarCAS", "LargeBlob", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_9"};
        nectar::CasStore cas{alloc, dir.View()};

        constexpr size_t kSize = 128 * 1024;
        wax::Vector<uint8_t> big{alloc};
        for (size_t i = 0; i < kSize; ++i)
            big.PushBack(static_cast<uint8_t>(i & 0xFF));

        auto hash = cas.Store(wax::ByteSpan{big.Data(), big.Size()});
        larvae::AssertTrue(hash.IsValid());

        auto loaded = cas.Load(hash);
        larvae::AssertEqual(loaded.Size(), kSize);
        larvae::AssertTrue(std::memcmp(loaded.Data(), big.Data(), kSize) == 0);
    });

    auto t10 = larvae::RegisterTest("NectarCAS", "MultipleBlobs", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_10"};
        nectar::CasStore cas{alloc, dir.View()};

        const char* data1 = "blob one";
        const char* data2 = "blob two";
        const char* data3 = "blob three";

        auto h1 = cas.Store(wax::ByteSpan{data1, std::strlen(data1)});
        auto h2 = cas.Store(wax::ByteSpan{data2, std::strlen(data2)});
        auto h3 = cas.Store(wax::ByteSpan{data3, std::strlen(data3)});

        larvae::AssertTrue(h1 != h2);
        larvae::AssertTrue(h2 != h3);
        larvae::AssertTrue(h1 != h3);

        larvae::AssertTrue(cas.Contains(h1));
        larvae::AssertTrue(cas.Contains(h2));
        larvae::AssertTrue(cas.Contains(h3));
    });

    auto t11 = larvae::RegisterTest("NectarCAS", "HashDeterminism", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_11"};
        nectar::CasStore cas{alloc, dir.View()};

        const char* data = "deterministic";
        auto h1 = cas.Store(wax::ByteSpan{data, std::strlen(data)});

        // Hash from ContentHash directly should match
        auto h2 = nectar::ContentHash::FromData(data, std::strlen(data));
        larvae::AssertTrue(h1 == h2);
    });

    auto t12 = larvae::RegisterTest("NectarCAS", "RootDir", []() {
        auto& alloc = GetCasAlloc();
        TempDir dir{"nectar_cas_test_12"};
        nectar::CasStore cas{alloc, dir.View()};

        larvae::AssertTrue(cas.RootDir().Size() > 0);
    });

}
