#include <larvae/larvae.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/vfs/memory_mount.h>
#include <comb/default_allocator.h>
#include <cstring>

namespace {

    auto& GetVfsAlloc()
    {
        static comb::ModuleAllocator alloc{"TestVFS", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // Basic mount + read
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarVFS", "MountAndRead", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        const char* data = "hello vfs";
        mem.AddFile("test.txt", wax::ByteSpan{data, std::strlen(data)});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        auto buf = vfs.ReadSync("test.txt");
        larvae::AssertEqual(buf.Size(), std::strlen(data));
        larvae::AssertTrue(std::memcmp(buf.Data(), data, buf.Size()) == 0);
    });

    auto t2 = larvae::RegisterTest("NectarVFS", "ReadFromMountedPrefix", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        const char* data = "texture data";
        mem.AddFile("hero.png", wax::ByteSpan{data, std::strlen(data)});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("textures", &mem);

        auto buf = vfs.ReadSync("textures/hero.png");
        larvae::AssertEqual(buf.Size(), std::strlen(data));
    });

    auto t3 = larvae::RegisterTest("NectarVFS", "ReadNotFound", []() {
        auto& alloc = GetVfsAlloc();
        nectar::VirtualFilesystem vfs{alloc};
        auto buf = vfs.ReadSync("nothing.txt");
        larvae::AssertEqual(buf.Size(), size_t{0});
    });

    // =========================================================================
    // Exists / Stat
    // =========================================================================

    auto t4 = larvae::RegisterTest("NectarVFS", "ExistsTrue", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("a.txt", wax::ByteSpan{});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);
        larvae::AssertTrue(vfs.Exists("a.txt"));
    });

    auto t5 = larvae::RegisterTest("NectarVFS", "ExistsFalse", []() {
        auto& alloc = GetVfsAlloc();
        nectar::VirtualFilesystem vfs{alloc};
        larvae::AssertFalse(vfs.Exists("a.txt"));
    });

    auto t6 = larvae::RegisterTest("NectarVFS", "StatFromMount", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        uint8_t data[64] = {};
        mem.AddFile("data.bin", wax::ByteSpan{data, sizeof(data)});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        auto info = vfs.Stat("data.bin");
        larvae::AssertTrue(info.exists);
        larvae::AssertEqual(info.size, size_t{64});
    });

    // =========================================================================
    // Priority overlay
    // =========================================================================

    auto t7 = larvae::RegisterTest("NectarVFS", "PriorityHigherWins", []() {
        auto& alloc = GetVfsAlloc();

        nectar::MemoryMountSource base{alloc};
        const char* base_data = "base";
        base.AddFile("config.txt", wax::ByteSpan{base_data, 4});

        nectar::MemoryMountSource overlay{alloc};
        const char* mod_data = "modded!";
        overlay.AddFile("config.txt", wax::ByteSpan{mod_data, 7});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &base, 0);
        vfs.Mount("", &overlay, 10);  // higher priority

        auto buf = vfs.ReadSync("config.txt");
        larvae::AssertEqual(buf.Size(), size_t{7});
        larvae::AssertTrue(std::memcmp(buf.Data(), mod_data, 7) == 0);
    });

    auto t8 = larvae::RegisterTest("NectarVFS", "PriorityFallback", []() {
        auto& alloc = GetVfsAlloc();

        nectar::MemoryMountSource base{alloc};
        const char* data = "only in base";
        base.AddFile("base_only.txt", wax::ByteSpan{data, std::strlen(data)});

        nectar::MemoryMountSource overlay{alloc};
        // overlay doesn't have base_only.txt

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &base, 0);
        vfs.Mount("", &overlay, 10);

        // Should fall through to base
        auto buf = vfs.ReadSync("base_only.txt");
        larvae::AssertEqual(buf.Size(), std::strlen(data));
    });

    // =========================================================================
    // Nested mount points
    // =========================================================================

    auto t9 = larvae::RegisterTest("NectarVFS", "NestedMountPoints", []() {
        auto& alloc = GetVfsAlloc();

        nectar::MemoryMountSource tex{alloc};
        tex.AddFile("hero.png", wax::ByteSpan{"tex", 3});

        nectar::MemoryMountSource mesh{alloc};
        mesh.AddFile("sword.glb", wax::ByteSpan{"mesh", 4});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("assets/textures", &tex);
        vfs.Mount("assets/meshes", &mesh);

        larvae::AssertTrue(vfs.Exists("assets/textures/hero.png"));
        larvae::AssertTrue(vfs.Exists("assets/meshes/sword.glb"));
        larvae::AssertFalse(vfs.Exists("assets/textures/sword.glb"));
    });

    // =========================================================================
    // Unmount
    // =========================================================================

    auto t10 = larvae::RegisterTest("NectarVFS", "UnmountRemovesSource", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("a.txt", wax::ByteSpan{});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);
        larvae::AssertTrue(vfs.Exists("a.txt"));

        vfs.Unmount("", &mem);
        larvae::AssertFalse(vfs.Exists("a.txt"));
        larvae::AssertEqual(vfs.MountCount(), size_t{0});
    });

    auto t11 = larvae::RegisterTest("NectarVFS", "UnmountNonExistent", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        nectar::VirtualFilesystem vfs{alloc};
        vfs.Unmount("", &mem);  // should not crash
        larvae::AssertEqual(vfs.MountCount(), size_t{0});
    });

    // =========================================================================
    // Path normalization
    // =========================================================================

    auto t12 = larvae::RegisterTest("NectarVFS", "PathNormalization", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("data.txt", wax::ByteSpan{"ok", 2});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        // Backslash and uppercase should be normalized
        larvae::AssertTrue(vfs.Exists("DATA.TXT"));
    });

    auto t13 = larvae::RegisterTest("NectarVFS", "RootMountMatchesAll", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("deep/nested/file.txt", wax::ByteSpan{"x", 1});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        larvae::AssertTrue(vfs.Exists("deep/nested/file.txt"));
    });

    // =========================================================================
    // Partial prefix no match
    // =========================================================================

    auto t14 = larvae::RegisterTest("NectarVFS", "PartialPrefixNoMatch", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("data.txt", wax::ByteSpan{});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("assets", &mem);

        // "assets2/data.txt" should NOT match mount point "assets"
        larvae::AssertFalse(vfs.Exists("assets2/data.txt"));
    });

    // =========================================================================
    // ListDirectory
    // =========================================================================

    auto t15 = larvae::RegisterTest("NectarVFS", "ListDirectoryMerge", []() {
        auto& alloc = GetVfsAlloc();

        nectar::MemoryMountSource base{alloc};
        base.AddFile("a.txt", wax::ByteSpan{});
        base.AddFile("b.txt", wax::ByteSpan{});

        nectar::MemoryMountSource overlay{alloc};
        overlay.AddFile("b.txt", wax::ByteSpan{});  // same name, different mount
        overlay.AddFile("c.txt", wax::ByteSpan{});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &base, 0);
        vfs.Mount("", &overlay, 10);

        wax::Vector<nectar::DirectoryEntry> entries{alloc};
        vfs.ListDirectory("", entries);

        // Should have a.txt, b.txt, c.txt (b.txt deduped)
        larvae::AssertEqual(entries.Size(), size_t{3});
    });

    auto t16 = larvae::RegisterTest("NectarVFS", "MountCount", []() {
        auto& alloc = GetVfsAlloc();
        nectar::MemoryMountSource a{alloc};
        nectar::MemoryMountSource b{alloc};

        nectar::VirtualFilesystem vfs{alloc};
        larvae::AssertEqual(vfs.MountCount(), size_t{0});

        vfs.Mount("", &a);
        vfs.Mount("textures", &b);
        larvae::AssertEqual(vfs.MountCount(), size_t{2});
    });

}
