#define _CRT_SECURE_NO_WARNINGS
#include <larvae/larvae.h>
#include <nectar/pak/pak_builder.h>
#include <nectar/pak/pak_reader.h>
#include <nectar/pak/npak_format.h>
#include <nectar/core/content_hash.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <cstdio>
#include <filesystem>

namespace {

    auto& GetPakRtAlloc()
    {
        static comb::ModuleAllocator alloc{"TestPakRT", 8 * 1024 * 1024};
        return alloc.Get();
    }

    const char* TempPakPath()
    {
        static std::string path =
            (std::filesystem::temp_directory_path() / "hive_test_output.npak").string();
        return path.c_str();
    }

    void CleanupTempPak()
    {
        std::remove(TempPakPath());
    }

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarPakRT", "BuildAndReadSingleBlob", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        uint8_t data[] = {10, 20, 30, 40, 50, 60, 70, 80};
        nectar::ContentHash hash = nectar::ContentHash::FromData(data, sizeof(data));

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(hash, wax::ByteSpan{data, sizeof(data)},
                            nectar::CompressionMethod::None);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);
        larvae::AssertTrue(reader->Contains(hash));

        auto loaded = reader->Read(hash, alloc);
        larvae::AssertEqual(loaded.Size(), sizeof(data));
        larvae::AssertTrue(std::memcmp(loaded.Data(), data, sizeof(data)) == 0);

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t2 = larvae::RegisterTest("NectarPakRT", "BuildAndReadMultipleBlobs", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        uint8_t d1[] = {1, 2, 3};
        uint8_t d2[] = {4, 5, 6, 7};
        uint8_t d3[] = {8, 9};
        auto h1 = nectar::ContentHash::FromData(d1, sizeof(d1));
        auto h2 = nectar::ContentHash::FromData(d2, sizeof(d2));
        auto h3 = nectar::ContentHash::FromData(d3, sizeof(d3));

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(h1, {d1, sizeof(d1)}, nectar::CompressionMethod::None);
            builder.AddBlob(h2, {d2, sizeof(d2)}, nectar::CompressionMethod::None);
            builder.AddBlob(h3, {d3, sizeof(d3)}, nectar::CompressionMethod::None);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);
        larvae::AssertEqual(reader->AssetCount(), size_t{3});

        auto l1 = reader->Read(h1, alloc);
        larvae::AssertEqual(l1.Size(), sizeof(d1));
        larvae::AssertTrue(std::memcmp(l1.Data(), d1, sizeof(d1)) == 0);

        auto l2 = reader->Read(h2, alloc);
        larvae::AssertEqual(l2.Size(), sizeof(d2));
        larvae::AssertTrue(std::memcmp(l2.Data(), d2, sizeof(d2)) == 0);

        auto l3 = reader->Read(h3, alloc);
        larvae::AssertEqual(l3.Size(), sizeof(d3));
        larvae::AssertTrue(std::memcmp(l3.Data(), d3, sizeof(d3)) == 0);

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t3 = larvae::RegisterTest("NectarPakRT", "BuildAndReadLargeBlob", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        // 200KB — spans multiple 64KB blocks
        constexpr size_t kSize = 200 * 1024;
        wax::ByteBuffer<> data{alloc};
        data.Resize(kSize);
        for (size_t i = 0; i < kSize; ++i)
            data.Data()[i] = static_cast<uint8_t>(i % 251);

        auto hash = nectar::ContentHash::FromData(data.View());

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(hash, data.View(), nectar::CompressionMethod::None);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);
        // 200KB / 64KB = 4 blocks (ceil)
        larvae::AssertTrue(reader->BlockCount() >= 4);

        auto loaded = reader->Read(hash, alloc);
        larvae::AssertEqual(loaded.Size(), kSize);
        larvae::AssertTrue(std::memcmp(loaded.Data(), data.Data(), kSize) == 0);

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t4 = larvae::RegisterTest("NectarPakRT", "BuildLZ4", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        // Compressible data
        constexpr size_t kSize = 4096;
        uint8_t data[kSize];
        for (size_t i = 0; i < kSize; ++i)
            data[i] = static_cast<uint8_t>(i % 3);

        auto hash = nectar::ContentHash::FromData(data, kSize);

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(hash, {data, kSize}, nectar::CompressionMethod::LZ4);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);

        auto loaded = reader->Read(hash, alloc);
        larvae::AssertEqual(loaded.Size(), kSize);
        larvae::AssertTrue(std::memcmp(loaded.Data(), data, kSize) == 0);

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t5 = larvae::RegisterTest("NectarPakRT", "BuildZstd", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        constexpr size_t kSize = 4096;
        uint8_t data[kSize];
        for (size_t i = 0; i < kSize; ++i)
            data[i] = static_cast<uint8_t>(i % 5);

        auto hash = nectar::ContentHash::FromData(data, kSize);

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(hash, {data, kSize}, nectar::CompressionMethod::Zstd);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);

        auto loaded = reader->Read(hash, alloc);
        larvae::AssertEqual(loaded.Size(), kSize);
        larvae::AssertTrue(std::memcmp(loaded.Data(), data, kSize) == 0);

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t6 = larvae::RegisterTest("NectarPakRT", "BuildMixedCompression", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        constexpr size_t kSize = 2048;
        uint8_t d1[kSize], d2[kSize], d3[kSize];
        for (size_t i = 0; i < kSize; ++i)
        {
            d1[i] = static_cast<uint8_t>(i % 2);
            d2[i] = static_cast<uint8_t>(i % 4);
            d3[i] = static_cast<uint8_t>(i % 6);
        }

        auto h1 = nectar::ContentHash::FromData(d1, kSize);
        auto h2 = nectar::ContentHash::FromData(d2, kSize);
        auto h3 = nectar::ContentHash::FromData(d3, kSize);

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(h1, {d1, kSize}, nectar::CompressionMethod::LZ4);
            builder.AddBlob(h2, {d2, kSize}, nectar::CompressionMethod::Zstd);
            builder.AddBlob(h3, {d3, kSize}, nectar::CompressionMethod::None);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);

        auto l1 = reader->Read(h1, alloc);
        larvae::AssertEqual(l1.Size(), kSize);
        larvae::AssertTrue(std::memcmp(l1.Data(), d1, kSize) == 0);

        auto l2 = reader->Read(h2, alloc);
        larvae::AssertEqual(l2.Size(), kSize);
        larvae::AssertTrue(std::memcmp(l2.Data(), d2, kSize) == 0);

        auto l3 = reader->Read(h3, alloc);
        larvae::AssertEqual(l3.Size(), kSize);
        larvae::AssertTrue(std::memcmp(l3.Data(), d3, kSize) == 0);

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t7 = larvae::RegisterTest("NectarPakRT", "ReaderContains", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        uint8_t data[] = {42};
        auto hash = nectar::ContentHash::FromData(data, sizeof(data));
        auto missing = nectar::ContentHash{0xDEAD, 0xBEEF};

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(hash, {data, sizeof(data)}, nectar::CompressionMethod::None);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);
        larvae::AssertTrue(reader->Contains(hash));
        larvae::AssertTrue(!reader->Contains(missing));

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t8 = larvae::RegisterTest("NectarPakRT", "ReaderNotFound", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        uint8_t data[] = {1, 2};
        auto hash = nectar::ContentHash::FromData(data, sizeof(data));

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(hash, {data, sizeof(data)}, nectar::CompressionMethod::None);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);

        auto loaded = reader->Read(nectar::ContentHash{0x1234, 0x5678}, alloc);
        larvae::AssertEqual(loaded.Size(), size_t{0});

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t9 = larvae::RegisterTest("NectarPakRT", "ReaderBadFile", []() {
        auto& alloc = GetPakRtAlloc();

        // Write garbage to file
        {
            FILE* f = std::fopen(TempPakPath(), "wb");
            const char* garbage = "not a pak file";
            std::fwrite(garbage, 1, 14, f);
            std::fclose(f);
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader == nullptr);

        CleanupTempPak();
    });

    auto t10 = larvae::RegisterTest("NectarPakRT", "ReaderNonExistentFile", []() {
        auto& alloc = GetPakRtAlloc();
        auto* reader = nectar::PakReader::Open("definitely_not_a_real_file.npak", alloc);
        larvae::AssertTrue(reader == nullptr);
    });

    auto t11 = larvae::RegisterTest("NectarPakRT", "ManifestInPak", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        uint8_t d1[] = {10, 20, 30};
        uint8_t d2[] = {40, 50, 60, 70};
        auto h1 = nectar::ContentHash::FromData(d1, sizeof(d1));
        auto h2 = nectar::ContentHash::FromData(d2, sizeof(d2));

        nectar::AssetManifest manifest{alloc};
        manifest.Add("textures/hero.png", h1);
        manifest.Add("meshes/sword.glb", h2);

        {
            nectar::PakBuilder builder{alloc};
            builder.AddBlob(h1, {d1, sizeof(d1)}, nectar::CompressionMethod::None);
            builder.AddBlob(h2, {d2, sizeof(d2)}, nectar::CompressionMethod::None);
            builder.SetManifest(manifest);
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);

        const auto* loaded_manifest = reader->GetManifest();
        larvae::AssertTrue(loaded_manifest != nullptr);
        larvae::AssertEqual(loaded_manifest->Count(), size_t{2});

        auto* mh1 = loaded_manifest->Find("textures/hero.png");
        larvae::AssertTrue(mh1 != nullptr);
        larvae::AssertTrue(*mh1 == h1);

        auto* mh2 = loaded_manifest->Find("meshes/sword.glb");
        larvae::AssertTrue(mh2 != nullptr);
        larvae::AssertTrue(*mh2 == h2);

        // Can also read the actual data
        auto ld1 = reader->Read(h1, alloc);
        larvae::AssertEqual(ld1.Size(), sizeof(d1));
        larvae::AssertTrue(std::memcmp(ld1.Data(), d1, sizeof(d1)) == 0);

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

    auto t12 = larvae::RegisterTest("NectarPakRT", "BuildEmpty", []() {
        auto& alloc = GetPakRtAlloc();
        CleanupTempPak();

        {
            nectar::PakBuilder builder{alloc};
            larvae::AssertTrue(builder.Build(TempPakPath()));
        }

        auto* reader = nectar::PakReader::Open(TempPakPath(), alloc);
        larvae::AssertTrue(reader != nullptr);
        larvae::AssertEqual(reader->AssetCount(), size_t{0});
        larvae::AssertEqual(reader->BlockCount(), size_t{0});

        reader->~PakReader();
        alloc.Deallocate(reader);
        CleanupTempPak();
    });

} // namespace
