#include <larvae/larvae.h>
#include <nectar/vfs/memory_mount.h>
#include <comb/default_allocator.h>
#include <cstring>

namespace {

    auto& GetMemMountAlloc()
    {
        static comb::ModuleAllocator alloc{"TestMemMount", 2 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // Read / Exists / Stat
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarMemoryMount", "AddAndRead", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};

        const char* content = "hello world";
        mount.AddFile("test.txt", wax::ByteSpan{content, std::strlen(content)});

        auto buf = mount.ReadFile("test.txt", alloc);
        larvae::AssertEqual(buf.Size(), std::strlen(content));
        larvae::AssertTrue(std::memcmp(buf.Data(), content, buf.Size()) == 0);
    });

    auto t2 = larvae::RegisterTest("NectarMemoryMount", "ReadNonExistent", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};

        auto buf = mount.ReadFile("nope.txt", alloc);
        larvae::AssertEqual(buf.Size(), size_t{0});
    });

    auto t3 = larvae::RegisterTest("NectarMemoryMount", "ExistsTrue", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};
        mount.AddFile("a.txt", wax::ByteSpan{});
        larvae::AssertTrue(mount.Exists("a.txt"));
    });

    auto t4 = larvae::RegisterTest("NectarMemoryMount", "ExistsFalse", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};
        larvae::AssertFalse(mount.Exists("a.txt"));
    });

    auto t5 = larvae::RegisterTest("NectarMemoryMount", "StatSize", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};

        uint8_t data[16] = {};
        mount.AddFile("data.bin", wax::ByteSpan{data, sizeof(data)});

        auto info = mount.Stat("data.bin");
        larvae::AssertTrue(info.exists);
        larvae::AssertEqual(info.size, size_t{16});
    });

    auto t6 = larvae::RegisterTest("NectarMemoryMount", "StatNonExistent", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};

        auto info = mount.Stat("nope.bin");
        larvae::AssertFalse(info.exists);
        larvae::AssertEqual(info.size, size_t{0});
    });

    // =========================================================================
    // Remove
    // =========================================================================

    auto t7 = larvae::RegisterTest("NectarMemoryMount", "RemoveFile", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};
        mount.AddFile("a.txt", wax::ByteSpan{});

        larvae::AssertTrue(mount.RemoveFile("a.txt"));
        larvae::AssertFalse(mount.Exists("a.txt"));
    });

    auto t8 = larvae::RegisterTest("NectarMemoryMount", "RemoveNonExistent", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};
        larvae::AssertFalse(mount.RemoveFile("a.txt"));
    });

    // =========================================================================
    // Overwrite
    // =========================================================================

    auto t9 = larvae::RegisterTest("NectarMemoryMount", "OverwriteFile", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};

        const char* v1 = "old";
        const char* v2 = "new data";
        mount.AddFile("f.txt", wax::ByteSpan{v1, 3});
        mount.AddFile("f.txt", wax::ByteSpan{v2, 8});

        auto buf = mount.ReadFile("f.txt", alloc);
        larvae::AssertEqual(buf.Size(), size_t{8});
        larvae::AssertTrue(std::memcmp(buf.Data(), v2, 8) == 0);
    });

    // =========================================================================
    // ListDirectory
    // =========================================================================

    auto t10 = larvae::RegisterTest("NectarMemoryMount", "ListDirectory", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};

        mount.AddFile("textures/hero.png", wax::ByteSpan{});
        mount.AddFile("textures/villain.png", wax::ByteSpan{});
        mount.AddFile("textures/sub/deep.png", wax::ByteSpan{});
        mount.AddFile("meshes/sword.glb", wax::ByteSpan{});

        wax::Vector<nectar::DirectoryEntry> entries{alloc};
        mount.ListDirectory("textures", entries, alloc);

        // Should have: hero.png (file), villain.png (file), sub (dir)
        larvae::AssertEqual(entries.Size(), size_t{3});

        // Verify all names are present
        bool found_hero = false, found_villain = false, found_sub = false;
        for (size_t i = 0; i < entries.Size(); ++i)
        {
            if (entries[i].name.View().Equals("hero.png"))
            {
                found_hero = true;
                larvae::AssertFalse(entries[i].is_directory);
            }
            if (entries[i].name.View().Equals("villain.png"))
            {
                found_villain = true;
                larvae::AssertFalse(entries[i].is_directory);
            }
            if (entries[i].name.View().Equals("sub"))
            {
                found_sub = true;
                larvae::AssertTrue(entries[i].is_directory);
            }
        }
        larvae::AssertTrue(found_hero);
        larvae::AssertTrue(found_villain);
        larvae::AssertTrue(found_sub);
    });

    auto t11 = larvae::RegisterTest("NectarMemoryMount", "ListEmpty", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};

        wax::Vector<nectar::DirectoryEntry> entries{alloc};
        mount.ListDirectory("nowhere", entries, alloc);
        larvae::AssertEqual(entries.Size(), size_t{0});
    });

    auto t12 = larvae::RegisterTest("NectarMemoryMount", "FileCount", []() {
        auto& alloc = GetMemMountAlloc();
        nectar::MemoryMountSource mount{alloc};

        larvae::AssertEqual(mount.FileCount(), size_t{0});
        mount.AddFile("a", wax::ByteSpan{});
        mount.AddFile("b", wax::ByteSpan{});
        larvae::AssertEqual(mount.FileCount(), size_t{2});
    });

}
