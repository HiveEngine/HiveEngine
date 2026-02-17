#define _CRT_SECURE_NO_WARNINGS
#include <larvae/larvae.h>
#include <nectar/io/mapped_file.h>
#include <nectar/vfs/mmap_mount.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <comb/default_allocator.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {

    auto& GetMmapAlloc()
    {
        static comb::ModuleAllocator alloc{"TestMmap", 4 * 1024 * 1024};
        return alloc.Get();
    }

    const char* MmapTestDir()
    {
        static std::string path =
            (std::filesystem::temp_directory_path() / "hive_test_mmap_dir").string();
        return path.c_str();
    }

    void SetupMmapDir()
    {
        std::error_code ec;
        std::filesystem::create_directories(MmapTestDir(), ec);
    }

    void CleanupMmapDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(MmapTestDir(), ec);
    }

    void WriteTestFile(const char* relative, const char* content)
    {
        std::string path = std::string(MmapTestDir()) + "/" + relative;

        // Create parent dirs if needed
        auto parent = std::filesystem::path(path).parent_path();
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);

        FILE* f = std::fopen(path.c_str(), "wb");
        if (f)
        {
            std::fwrite(content, 1, std::strlen(content), f);
            std::fclose(f);
        }
    }

    std::string FullPath(const char* relative)
    {
        return std::string(MmapTestDir()) + "/" + relative;
    }

    // =========================================================================
    // MappedFile
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarMmap", "MappedFileOpenValid", []() {
        SetupMmapDir();
        WriteTestFile("hello.txt", "Hello World!");

        auto path = FullPath("hello.txt");
        auto mapped = nectar::MappedFile::Open(
            wax::StringView{path.c_str(), path.size()});

        larvae::AssertTrue(mapped.IsValid());
        larvae::AssertEqual(mapped.Size(), std::strlen("Hello World!"));

        CleanupMmapDir();
    });

    auto t2 = larvae::RegisterTest("NectarMmap", "MappedFileOpenInvalid", []() {
        auto mapped = nectar::MappedFile::Open("nonexistent_file_that_does_not_exist.bin");
        larvae::AssertFalse(mapped.IsValid());
        larvae::AssertEqual(mapped.Size(), size_t{0});
        larvae::AssertTrue(mapped.Data() == nullptr);
    });

    auto t3 = larvae::RegisterTest("NectarMmap", "MappedFileDataCorrect", []() {
        SetupMmapDir();
        WriteTestFile("data.bin", "ABCDEFGH");

        auto path = FullPath("data.bin");
        auto mapped = nectar::MappedFile::Open(
            wax::StringView{path.c_str(), path.size()});

        larvae::AssertTrue(mapped.IsValid());
        larvae::AssertTrue(std::memcmp(mapped.Data(), "ABCDEFGH", 8) == 0);

        auto view = mapped.View();
        larvae::AssertEqual(view.Size(), size_t{8});
        larvae::AssertTrue(view.Data() == mapped.Data());

        CleanupMmapDir();
    });

    auto t4 = larvae::RegisterTest("NectarMmap", "MappedFileMoveSemantics", []() {
        SetupMmapDir();
        WriteTestFile("move.txt", "movable");

        auto path = FullPath("move.txt");
        auto a = nectar::MappedFile::Open(
            wax::StringView{path.c_str(), path.size()});
        larvae::AssertTrue(a.IsValid());

        // Move construct
        auto b = static_cast<nectar::MappedFile&&>(a);
        larvae::AssertFalse(a.IsValid());
        larvae::AssertTrue(b.IsValid());
        larvae::AssertEqual(b.Size(), std::strlen("movable"));

        // Move assign
        nectar::MappedFile c;
        c = static_cast<nectar::MappedFile&&>(b);
        larvae::AssertFalse(b.IsValid());
        larvae::AssertTrue(c.IsValid());
        larvae::AssertEqual(c.Size(), std::strlen("movable"));

        CleanupMmapDir();
    });

    // =========================================================================
    // MmapMountSource
    // =========================================================================

    auto t5 = larvae::RegisterTest("NectarMmap", "MmapMountReadFile", []() {
        auto& alloc = GetMmapAlloc();
        SetupMmapDir();
        WriteTestFile("textures/hero.png", "pixel_data_here");

        nectar::MmapMountSource mount{MmapTestDir(), alloc};
        auto buf = mount.ReadFile("textures/hero.png", alloc);
        larvae::AssertEqual(buf.Size(), std::strlen("pixel_data_here"));
        larvae::AssertTrue(std::memcmp(buf.Data(), "pixel_data_here", buf.Size()) == 0);

        CleanupMmapDir();
    });

    auto t6 = larvae::RegisterTest("NectarMmap", "MmapMountReadNotFound", []() {
        auto& alloc = GetMmapAlloc();
        SetupMmapDir();

        nectar::MmapMountSource mount{MmapTestDir(), alloc};
        auto buf = mount.ReadFile("missing.txt", alloc);
        larvae::AssertEqual(buf.Size(), size_t{0});

        CleanupMmapDir();
    });

    auto t7 = larvae::RegisterTest("NectarMmap", "MmapMountExists", []() {
        auto& alloc = GetMmapAlloc();
        SetupMmapDir();
        WriteTestFile("found.txt", "yes");

        nectar::MmapMountSource mount{MmapTestDir(), alloc};
        larvae::AssertTrue(mount.Exists("found.txt"));
        larvae::AssertFalse(mount.Exists("not_found.txt"));

        CleanupMmapDir();
    });

    auto t8 = larvae::RegisterTest("NectarMmap", "MmapMountInVFS", []() {
        auto& alloc = GetMmapAlloc();
        SetupMmapDir();
        WriteTestFile("a.txt", "aaa");
        WriteTestFile("b.txt", "bbbbb");

        nectar::MmapMountSource mount{MmapTestDir(), alloc};
        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("data", &mount);

        larvae::AssertTrue(vfs.Exists("data/a.txt"));
        larvae::AssertTrue(vfs.Exists("data/b.txt"));
        larvae::AssertFalse(vfs.Exists("data/c.txt"));

        auto buf = vfs.ReadSync("data/b.txt");
        larvae::AssertEqual(buf.Size(), size_t{5});
        larvae::AssertTrue(std::memcmp(buf.Data(), "bbbbb", 5) == 0);

        CleanupMmapDir();
    });

} // namespace
