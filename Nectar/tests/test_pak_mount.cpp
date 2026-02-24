#define _CRT_SECURE_NO_WARNINGS
#include <larvae/larvae.h>
#include <nectar/vfs/pak_mount.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/pak/pak_builder.h>
#include <nectar/pak/pak_reader.h>
#include <nectar/pak/asset_manifest.h>
#include <nectar/core/content_hash.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <cstdio>
#include <filesystem>

namespace {

    auto& GetPakMountAlloc()
    {
        static comb::ModuleAllocator alloc{"TestPakMount", 8 * 1024 * 1024};
        return alloc.Get();
    }

    const char* TempMountPakPath()
    {
        static std::string path =
            (std::filesystem::temp_directory_path() / "hive_test_mount.npak").string();
        return path.c_str();
    }

    void CleanupMountPak()
    {
        std::remove(TempMountPakPath());
    }

    // Helper: build a .npak with manifest from data pairs
    nectar::PakReader* BuildTestPak(comb::DefaultAllocator& alloc,
                                     const char* paths[], const char* datas[],
                                     size_t count)
    {
        CleanupMountPak();

        nectar::PakBuilder builder{alloc};
        nectar::AssetManifest manifest{alloc};

        for (size_t i = 0; i < count; ++i)
        {
            size_t len = std::strlen(datas[i]);
            auto hash = nectar::ContentHash::FromData(datas[i], len);
            builder.AddBlob(hash,
                wax::ByteSpan{reinterpret_cast<const uint8_t*>(datas[i]), len},
                nectar::CompressionMethod::None);
            manifest.Add(wax::StringView{paths[i], std::strlen(paths[i])}, hash);
        }

        builder.SetManifest(manifest);
        (void)builder.Build(TempMountPakPath());

        return nectar::PakReader::Open(
            wax::StringView{TempMountPakPath(), std::strlen(TempMountPakPath())}, alloc);
    }

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarPakMount", "ReadFileFound", []() {
        auto& alloc = GetPakMountAlloc();
        const char* paths[] = {"textures/hero.png"};
        const char* datas[] = {"pixel_data_here"};

        auto* reader = BuildTestPak(alloc, paths, datas, 1);
        larvae::AssertTrue(reader != nullptr);

        nectar::PakMountSource mount{reader, alloc};

        auto buf = mount.ReadFile("textures/hero.png", alloc);
        larvae::AssertEqual(buf.Size(), std::strlen("pixel_data_here"));
        larvae::AssertTrue(std::memcmp(buf.Data(), "pixel_data_here", buf.Size()) == 0);

        CleanupMountPak();
    });

    auto t2 = larvae::RegisterTest("NectarPakMount", "ReadFileNotFound", []() {
        auto& alloc = GetPakMountAlloc();
        const char* paths[] = {"a.txt"};
        const char* datas[] = {"aaa"};

        auto* reader = BuildTestPak(alloc, paths, datas, 1);
        larvae::AssertTrue(reader != nullptr);

        nectar::PakMountSource mount{reader, alloc};

        auto buf = mount.ReadFile("missing.txt", alloc);
        larvae::AssertEqual(buf.Size(), size_t{0});

        CleanupMountPak();
    });

    auto t3 = larvae::RegisterTest("NectarPakMount", "ExistsTrue", []() {
        auto& alloc = GetPakMountAlloc();
        const char* paths[] = {"models/sword.glb"};
        const char* datas[] = {"mesh_data"};

        auto* reader = BuildTestPak(alloc, paths, datas, 1);
        larvae::AssertTrue(reader != nullptr);

        nectar::PakMountSource mount{reader, alloc};
        larvae::AssertTrue(mount.Exists("models/sword.glb"));

        CleanupMountPak();
    });

    auto t4 = larvae::RegisterTest("NectarPakMount", "ExistsFalse", []() {
        auto& alloc = GetPakMountAlloc();
        const char* paths[] = {"a.txt"};
        const char* datas[] = {"x"};

        auto* reader = BuildTestPak(alloc, paths, datas, 1);
        larvae::AssertTrue(reader != nullptr);

        nectar::PakMountSource mount{reader, alloc};
        larvae::AssertFalse(mount.Exists("b.txt"));

        CleanupMountPak();
    });

    auto t5 = larvae::RegisterTest("NectarPakMount", "StatFound", []() {
        auto& alloc = GetPakMountAlloc();
        const char* paths[] = {"data.bin"};
        const char* datas[] = {"0123456789"};

        auto* reader = BuildTestPak(alloc, paths, datas, 1);
        larvae::AssertTrue(reader != nullptr);

        nectar::PakMountSource mount{reader, alloc};

        auto info = mount.Stat("data.bin");
        larvae::AssertTrue(info.exists);
        larvae::AssertEqual(info.size, size_t{10});

        CleanupMountPak();
    });

    auto t6 = larvae::RegisterTest("NectarPakMount", "StatNotFound", []() {
        auto& alloc = GetPakMountAlloc();
        const char* paths[] = {"a.txt"};
        const char* datas[] = {"x"};

        auto* reader = BuildTestPak(alloc, paths, datas, 1);
        larvae::AssertTrue(reader != nullptr);

        nectar::PakMountSource mount{reader, alloc};

        auto info = mount.Stat("missing.txt");
        larvae::AssertFalse(info.exists);
        larvae::AssertEqual(info.size, size_t{0});

        CleanupMountPak();
    });

    auto t7 = larvae::RegisterTest("NectarPakMount", "ListDirectoryBasic", []() {
        auto& alloc = GetPakMountAlloc();
        const char* paths[] = {
            "textures/hero.png",
            "textures/metal.png",
            "meshes/sword.glb"
        };
        const char* datas[] = {"a", "bb", "ccc"};

        auto* reader = BuildTestPak(alloc, paths, datas, 3);
        larvae::AssertTrue(reader != nullptr);

        nectar::PakMountSource mount{reader, alloc};

        // List root: should show "textures" (dir) and "meshes" (dir)
        wax::Vector<nectar::DirectoryEntry> entries{alloc};
        mount.ListDirectory("", entries, alloc);
        larvae::AssertEqual(entries.Size(), size_t{2});

        // List textures/: should show "hero.png" and "metal.png"
        wax::Vector<nectar::DirectoryEntry> tex_entries{alloc};
        mount.ListDirectory("textures", tex_entries, alloc);
        larvae::AssertEqual(tex_entries.Size(), size_t{2});

        CleanupMountPak();
    });

    auto t8 = larvae::RegisterTest("NectarPakMount", "MountInVFS", []() {
        auto& alloc = GetPakMountAlloc();
        const char* paths[] = {"hero.png", "metal.png"};
        const char* datas[] = {"hero_pixels", "metal_pixels"};

        auto* reader = BuildTestPak(alloc, paths, datas, 2);
        larvae::AssertTrue(reader != nullptr);

        nectar::PakMountSource mount{reader, alloc};

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("assets", &mount);

        larvae::AssertTrue(vfs.Exists("assets/hero.png"));
        larvae::AssertTrue(vfs.Exists("assets/metal.png"));
        larvae::AssertFalse(vfs.Exists("assets/missing.png"));

        auto buf = vfs.ReadSync("assets/hero.png");
        larvae::AssertEqual(buf.Size(), std::strlen("hero_pixels"));
        larvae::AssertTrue(std::memcmp(buf.Data(), "hero_pixels", buf.Size()) == 0);

        CleanupMountPak();
    });

} // namespace
