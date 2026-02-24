#define _CRT_SECURE_NO_WARNINGS
#include <larvae/larvae.h>
#include <nectar/vfs/disk_mount.h>
#include <comb/default_allocator.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {

    auto& GetDiskAlloc()
    {
        static comb::ModuleAllocator alloc{"TestDiskMount", 2 * 1024 * 1024};
        return alloc.Get();
    }

    struct TestDiskDir
    {
        std::filesystem::path path;

        TestDiskDir()
        {
            path = std::filesystem::temp_directory_path() / "nectar_disk_mount_test";
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            std::filesystem::create_directories(path);
        }

        ~TestDiskDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        const char* CStr() const
        {
            static thread_local std::string s;
            s = path.string();
            return s.c_str();
        }
    };

    TestDiskDir& GetTestDir()
    {
        static TestDiskDir dir;
        return dir;
    }

    void WriteTestFile(const char* relative, const void* data, size_t size)
    {
        auto& dir = GetTestDir();
        std::string full = dir.path.string() + "/" + relative;

        auto parent = std::filesystem::path{full}.parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);

        FILE* f = std::fopen(full.c_str(), "wb");
        if (f)
        {
            std::fwrite(data, 1, size, f);
            std::fclose(f);
        }
    }

    // =========================================================================
    // Basic operations
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarDiskMount", "ReadExistingFile", []() {
        auto& alloc = GetDiskAlloc();
        const char* content = "disk test data";
        WriteTestFile("read_test.txt", content, std::strlen(content));

        nectar::DiskMountSource mount{GetTestDir().CStr(), alloc};
        auto buf = mount.ReadFile("read_test.txt", alloc);
        larvae::AssertEqual(buf.Size(), std::strlen(content));
        larvae::AssertTrue(std::memcmp(buf.Data(), content, buf.Size()) == 0);
    });

    auto t2 = larvae::RegisterTest("NectarDiskMount", "ReadNonExistent", []() {
        auto& alloc = GetDiskAlloc();
        nectar::DiskMountSource mount{GetTestDir().CStr(), alloc};
        auto buf = mount.ReadFile("does_not_exist.txt", alloc);
        larvae::AssertEqual(buf.Size(), size_t{0});
    });

    auto t3 = larvae::RegisterTest("NectarDiskMount", "ExistsTrue", []() {
        auto& alloc = GetDiskAlloc();
        const char* data = "x";
        WriteTestFile("exists_test.txt", data, 1);

        nectar::DiskMountSource mount{GetTestDir().CStr(), alloc};
        larvae::AssertTrue(mount.Exists("exists_test.txt"));
    });

    auto t4 = larvae::RegisterTest("NectarDiskMount", "ExistsFalse", []() {
        auto& alloc = GetDiskAlloc();
        nectar::DiskMountSource mount{GetTestDir().CStr(), alloc};
        larvae::AssertFalse(mount.Exists("nope.txt"));
    });

    auto t5 = larvae::RegisterTest("NectarDiskMount", "StatSize", []() {
        auto& alloc = GetDiskAlloc();
        uint8_t data[42] = {};
        WriteTestFile("stat_test.bin", data, sizeof(data));

        nectar::DiskMountSource mount{GetTestDir().CStr(), alloc};
        auto info = mount.Stat("stat_test.bin");
        larvae::AssertTrue(info.exists);
        larvae::AssertEqual(info.size, size_t{42});
    });

    auto t6 = larvae::RegisterTest("NectarDiskMount", "StatNonExistent", []() {
        auto& alloc = GetDiskAlloc();
        nectar::DiskMountSource mount{GetTestDir().CStr(), alloc};
        auto info = mount.Stat("nope.bin");
        larvae::AssertFalse(info.exists);
    });

    auto t7 = larvae::RegisterTest("NectarDiskMount", "ListDirectory", []() {
        auto& alloc = GetDiskAlloc();
        WriteTestFile("listdir/a.txt", "a", 1);
        WriteTestFile("listdir/b.txt", "b", 1);

        nectar::DiskMountSource mount{GetTestDir().CStr(), alloc};
        wax::Vector<nectar::DirectoryEntry> entries{alloc};
        mount.ListDirectory("listdir", entries, alloc);

        larvae::AssertTrue(entries.Size() >= size_t{2});
    });

    auto t8 = larvae::RegisterTest("NectarDiskMount", "RootDir", []() {
        auto& alloc = GetDiskAlloc();
        nectar::DiskMountSource mount{"my/root", alloc};
        larvae::AssertTrue(mount.RootDir().Equals("my/root"));
    });

}
