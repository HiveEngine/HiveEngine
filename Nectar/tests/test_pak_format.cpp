#include <larvae/larvae.h>
#include <nectar/pak/crc32.h>
#include <nectar/pak/npak_format.h>
#include <nectar/pak/compression.h>
#include <nectar/pak/asset_manifest.h>
#include <comb/default_allocator.h>
#include <cstring>

namespace {

    auto& GetPakAlloc()
    {
        static comb::ModuleAllocator alloc{"TestPakFmt", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // CRC32
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarPakFmt", "Crc32Empty", []() {
        uint32_t crc = nectar::Crc32(nullptr, 0);
        // CRC32 of empty data = 0x00000000
        larvae::AssertEqual(crc, uint32_t{0x00000000});
    });

    auto t2 = larvae::RegisterTest("NectarPakFmt", "Crc32KnownValue", []() {
        // CRC32 of "123456789" = 0xCBF43926
        const char* data = "123456789";
        uint32_t crc = nectar::Crc32(data, 9);
        larvae::AssertEqual(crc, uint32_t{0xCBF43926});
    });

    auto t3 = larvae::RegisterTest("NectarPakFmt", "Crc32Incremental", []() {
        const char* data = "Hello, World!";
        size_t len = 13;

        // Full compute
        uint32_t full = nectar::Crc32(data, len);

        // Incremental: split at position 5
        uint32_t partial = nectar::Crc32Update(0xFFFFFFFF, data, 5);
        partial = nectar::Crc32Update(partial, data + 5, len - 5);
        partial ^= 0xFFFFFFFF;

        larvae::AssertEqual(full, partial);
    });

    // =========================================================================
    // Compression
    // =========================================================================

    auto t4 = larvae::RegisterTest("NectarPakFmt", "CompressDecompressLZ4", []() {
        auto& alloc = GetPakAlloc();

        // Compressible data (repeated pattern)
        uint8_t src[1024];
        for (size_t i = 0; i < sizeof(src); ++i)
            src[i] = static_cast<uint8_t>(i % 7);

        wax::ByteSpan input{src, sizeof(src)};
        auto compressed = nectar::Compress(input, nectar::CompressionMethod::LZ4, alloc);
        larvae::AssertTrue(compressed.Size() > 0);
        larvae::AssertTrue(compressed.Size() < sizeof(src));

        auto decompressed = nectar::Decompress(
            compressed.View(), sizeof(src), nectar::CompressionMethod::LZ4, alloc);
        larvae::AssertEqual(decompressed.Size(), sizeof(src));
        larvae::AssertTrue(std::memcmp(decompressed.Data(), src, sizeof(src)) == 0);
    });

    auto t5 = larvae::RegisterTest("NectarPakFmt", "CompressDecompressZstd", []() {
        auto& alloc = GetPakAlloc();

        uint8_t src[1024];
        for (size_t i = 0; i < sizeof(src); ++i)
            src[i] = static_cast<uint8_t>(i % 13);

        wax::ByteSpan input{src, sizeof(src)};
        auto compressed = nectar::Compress(input, nectar::CompressionMethod::Zstd, alloc);
        larvae::AssertTrue(compressed.Size() > 0);
        larvae::AssertTrue(compressed.Size() < sizeof(src));

        auto decompressed = nectar::Decompress(
            compressed.View(), sizeof(src), nectar::CompressionMethod::Zstd, alloc);
        larvae::AssertEqual(decompressed.Size(), sizeof(src));
        larvae::AssertTrue(std::memcmp(decompressed.Data(), src, sizeof(src)) == 0);
    });

    auto t6 = larvae::RegisterTest("NectarPakFmt", "CompressNone", []() {
        auto& alloc = GetPakAlloc();

        uint8_t src[] = {1, 2, 3, 4, 5};
        wax::ByteSpan input{src, sizeof(src)};

        auto compressed = nectar::Compress(input, nectar::CompressionMethod::None, alloc);
        larvae::AssertEqual(compressed.Size(), sizeof(src));
        larvae::AssertTrue(std::memcmp(compressed.Data(), src, sizeof(src)) == 0);
    });

    auto t7 = larvae::RegisterTest("NectarPakFmt", "CompressIncompressible", []() {
        auto& alloc = GetPakAlloc();

        // Very small random-ish data that won't compress well
        uint8_t src[16];
        for (size_t i = 0; i < sizeof(src); ++i)
            src[i] = static_cast<uint8_t>((i * 137 + 73) & 0xFF);

        wax::ByteSpan input{src, sizeof(src)};
        auto compressed = nectar::Compress(input, nectar::CompressionMethod::LZ4, alloc);
        // Should return empty (LZ4 header overhead > data)
        larvae::AssertEqual(compressed.Size(), size_t{0});
    });

    auto t8 = larvae::RegisterTest("NectarPakFmt", "CompressEmpty", []() {
        auto& alloc = GetPakAlloc();
        wax::ByteSpan input{static_cast<const uint8_t*>(nullptr), size_t{0}};
        auto compressed = nectar::Compress(input, nectar::CompressionMethod::LZ4, alloc);
        larvae::AssertEqual(compressed.Size(), size_t{0});
    });

    // =========================================================================
    // Format struct sizes (compile-time, but verify at runtime too)
    // =========================================================================

    auto t9 = larvae::RegisterTest("NectarPakFmt", "HeaderSize", []() {
        larvae::AssertEqual(sizeof(nectar::NpakHeader), size_t{32});
    });

    auto t10 = larvae::RegisterTest("NectarPakFmt", "AssetEntrySize", []() {
        larvae::AssertEqual(sizeof(nectar::NpakAssetEntry), size_t{28});
    });

    auto t11 = larvae::RegisterTest("NectarPakFmt", "BlockEntrySize", []() {
        larvae::AssertEqual(sizeof(nectar::NpakBlockEntry), size_t{13});
    });

    // =========================================================================
    // AssetManifest
    // =========================================================================

    auto t12 = larvae::RegisterTest("NectarPakFmt", "ManifestAddFind", []() {
        auto& alloc = GetPakAlloc();
        nectar::AssetManifest manifest{alloc};

        nectar::ContentHash h1{0x1111, 0x2222};
        manifest.Add("textures/hero.png", h1);

        auto* found = manifest.Find("textures/hero.png");
        larvae::AssertTrue(found != nullptr);
        larvae::AssertTrue(*found == h1);

        larvae::AssertTrue(manifest.Find("textures/missing.png") == nullptr);
    });

    auto t13 = larvae::RegisterTest("NectarPakFmt", "ManifestSerializeDeserialize", []() {
        auto& alloc = GetPakAlloc();
        nectar::AssetManifest manifest{alloc};

        nectar::ContentHash h1{0xAAAA, 0xBBBB};
        nectar::ContentHash h2{0xCCCC, 0xDDDD};
        manifest.Add("meshes/sword.glb", h1);
        manifest.Add("textures/metal.png", h2);

        auto serialized = manifest.Serialize(alloc);
        larvae::AssertTrue(serialized.Size() > 0);

        auto restored = nectar::AssetManifest::Deserialize(serialized.View(), alloc);
        larvae::AssertEqual(restored.Count(), size_t{2});

        auto* f1 = restored.Find("meshes/sword.glb");
        larvae::AssertTrue(f1 != nullptr);
        larvae::AssertTrue(*f1 == h1);

        auto* f2 = restored.Find("textures/metal.png");
        larvae::AssertTrue(f2 != nullptr);
        larvae::AssertTrue(*f2 == h2);
    });

    auto t14 = larvae::RegisterTest("NectarPakFmt", "ManifestEmpty", []() {
        auto& alloc = GetPakAlloc();
        nectar::AssetManifest manifest{alloc};
        larvae::AssertEqual(manifest.Count(), size_t{0});

        auto serialized = manifest.Serialize(alloc);
        // Should contain at least the count (4 bytes)
        larvae::AssertTrue(serialized.Size() >= sizeof(uint32_t));

        auto restored = nectar::AssetManifest::Deserialize(serialized.View(), alloc);
        larvae::AssertEqual(restored.Count(), size_t{0});
    });

    auto t15 = larvae::RegisterTest("NectarPakFmt", "ManifestMultipleEntries", []() {
        auto& alloc = GetPakAlloc();
        nectar::AssetManifest manifest{alloc};

        for (uint32_t i = 0; i < 20; ++i)
        {
            char path[64];
            int len = std::snprintf(path, sizeof(path), "asset_%u.bin", i);
            manifest.Add(wax::StringView{path, static_cast<size_t>(len)},
                         nectar::ContentHash{i, i * 100});
        }

        larvae::AssertEqual(manifest.Count(), size_t{20});

        // Roundtrip
        auto serialized = manifest.Serialize(alloc);
        auto restored = nectar::AssetManifest::Deserialize(serialized.View(), alloc);
        larvae::AssertEqual(restored.Count(), size_t{20});

        // Spot check
        auto* f = restored.Find("asset_7.bin");
        larvae::AssertTrue(f != nullptr);
        larvae::AssertTrue(*f == nectar::ContentHash{7, 700});
    });

} // namespace
