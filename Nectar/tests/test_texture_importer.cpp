#include <larvae/larvae.h>
#include <nectar/texture/texture_importer.h>
#include <nectar/texture/texture_data.h>
#include <nectar/pipeline/import_context.h>
#include <nectar/database/asset_database.h>
#include <nectar/hive/hive_document.h>
#include <nectar/core/asset_id.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <cstdlib>

namespace {

    auto& GetTexAlloc()
    {
        static comb::ModuleAllocator alloc{"TestTexture", 8 * 1024 * 1024};
        return alloc.Get();
    }

    nectar::AssetId MakeId(uint64_t v)
    {
        uint8_t bytes[16] = {};
        std::memcpy(bytes, &v, sizeof(v));
        return nectar::AssetId::FromBytes(bytes);
    }

    // Minimal valid 4x4 RGBA BMP file (uncompressed, 32-bit)
    // BMP is easier to generate in memory than PNG
    struct BmpBuilder
    {
        wax::Vector<uint8_t> data;

        explicit BmpBuilder(comb::DefaultAllocator& alloc)
            : data{alloc}
        {}

        void Build(uint32_t w, uint32_t h, const uint8_t* rgba_pixels)
        {
            // BMP header (14 bytes) + DIB header (40 bytes BITMAPINFOHEADER)
            uint32_t pixel_data_size = w * h * 4;
            uint32_t file_size = 14 + 40 + pixel_data_size;

            data.Reserve(file_size);

            // BMP file header (14 bytes)
            PushU8('B'); PushU8('M');
            PushU32(file_size);
            PushU16(0); PushU16(0);     // reserved
            PushU32(14 + 40);           // pixel data offset

            // BITMAPINFOHEADER (40 bytes)
            PushU32(40);                // header size
            PushI32(static_cast<int32_t>(w));
            PushI32(-static_cast<int32_t>(h));  // negative = top-down
            PushU16(1);                 // planes
            PushU16(32);                // bits per pixel
            PushU32(0);                 // compression (BI_RGB)
            PushU32(pixel_data_size);
            PushI32(2835); PushI32(2835); // px/m
            PushU32(0); PushU32(0);     // colors

            // Pixel data: BMP 32-bit is BGRA, not RGBA
            for (uint32_t i = 0; i < w * h; ++i)
            {
                PushU8(rgba_pixels[i * 4 + 2]); // B
                PushU8(rgba_pixels[i * 4 + 1]); // G
                PushU8(rgba_pixels[i * 4 + 0]); // R
                PushU8(rgba_pixels[i * 4 + 3]); // A
            }
        }

        void PushU8(uint8_t v) { data.PushBack(v); }
        void PushU16(uint16_t v) { PushU8(v & 0xFF); PushU8((v >> 8) & 0xFF); }
        void PushU32(uint32_t v) { PushU16(v & 0xFFFF); PushU16((v >> 16) & 0xFFFF); }
        void PushI32(int32_t v) { PushU32(static_cast<uint32_t>(v)); }
    };

    wax::ByteSpan MakeBmp(BmpBuilder& b, uint32_t w, uint32_t h, const uint8_t* rgba)
    {
        b.Build(w, h, rgba);
        return wax::ByteSpan{b.data.Data(), b.data.Size()};
    }

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarTexture", "DecodeBMP", []() {
        auto& alloc = GetTexAlloc();
        // 4x4 solid red
        uint8_t pixels[4 * 4 * 4];
        for (int i = 0; i < 16; ++i)
        {
            pixels[i * 4 + 0] = 255; // R
            pixels[i * 4 + 1] = 0;   // G
            pixels[i * 4 + 2] = 0;   // B
            pixels[i * 4 + 3] = 255; // A
        }

        BmpBuilder bmp{alloc};
        auto span = MakeBmp(bmp, 4, 4, pixels);

        nectar::TextureImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(1)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(span, settings, ctx);
        larvae::AssertTrue(result.success);
        larvae::AssertTrue(result.intermediate_data.Size() > sizeof(nectar::NtexHeader));

        // Parse header
        auto* header = reinterpret_cast<const nectar::NtexHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->magic, nectar::kNtexMagic);
        larvae::AssertEqual(header->width, uint32_t{4});
        larvae::AssertEqual(header->height, uint32_t{4});
        larvae::AssertEqual(header->channels, uint32_t{4});
    });

    auto t2 = larvae::RegisterTest("NectarTexture", "MipMapGeneration", []() {
        auto& alloc = GetTexAlloc();
        // 8x8 white image
        uint8_t pixels[8 * 8 * 4];
        std::memset(pixels, 0xFF, sizeof(pixels));

        BmpBuilder bmp{alloc};
        auto span = MakeBmp(bmp, 8, 8, pixels);

        nectar::TextureImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(2)};
        nectar::HiveDocument settings{alloc};
        // default: generate_mipmaps = true

        auto result = importer.Import(span, settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NtexHeader*>(result.intermediate_data.Data());
        // 8x8 -> 4x4 -> 2x2 -> 1x1 = 4 mip levels
        larvae::AssertEqual(header->mip_count, uint8_t{4});

        // Check mip table
        auto* mips = reinterpret_cast<const nectar::TextureMipLevel*>(
            result.intermediate_data.Data() + sizeof(nectar::NtexHeader));
        larvae::AssertEqual(mips[0].width, uint32_t{8});
        larvae::AssertEqual(mips[0].height, uint32_t{8});
        larvae::AssertEqual(mips[1].width, uint32_t{4});
        larvae::AssertEqual(mips[1].height, uint32_t{4});
        larvae::AssertEqual(mips[2].width, uint32_t{2});
        larvae::AssertEqual(mips[2].height, uint32_t{2});
        larvae::AssertEqual(mips[3].width, uint32_t{1});
        larvae::AssertEqual(mips[3].height, uint32_t{1});
    });

    auto t3 = larvae::RegisterTest("NectarTexture", "MipMapDisabled", []() {
        auto& alloc = GetTexAlloc();
        uint8_t pixels[4 * 4 * 4];
        std::memset(pixels, 0x80, sizeof(pixels));

        BmpBuilder bmp{alloc};
        auto span = MakeBmp(bmp, 4, 4, pixels);

        nectar::TextureImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(3)};
        nectar::HiveDocument settings{alloc};
        settings.SetValue("import", "generate_mipmaps",
                          nectar::HiveValue::MakeBool(false));

        auto result = importer.Import(span, settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NtexHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->mip_count, uint8_t{1});
    });

    auto t4 = larvae::RegisterTest("NectarTexture", "MaxSizeClamp", []() {
        auto& alloc = GetTexAlloc();
        // 16x16 image
        constexpr uint32_t kSrc = 16;
        uint8_t pixels[kSrc * kSrc * 4];
        std::memset(pixels, 0x40, sizeof(pixels));

        BmpBuilder bmp{alloc};
        auto span = MakeBmp(bmp, kSrc, kSrc, pixels);

        nectar::TextureImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(4)};
        nectar::HiveDocument settings{alloc};
        settings.SetValue("import", "max_size", nectar::HiveValue::MakeInt(8));
        settings.SetValue("import", "generate_mipmaps",
                          nectar::HiveValue::MakeBool(false));

        auto result = importer.Import(span, settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NtexHeader*>(result.intermediate_data.Data());
        larvae::AssertTrue(header->width <= 8);
        larvae::AssertTrue(header->height <= 8);
    });

    auto t5 = larvae::RegisterTest("NectarTexture", "SrgbFlag", []() {
        auto& alloc = GetTexAlloc();
        uint8_t pixels[2 * 2 * 4];
        std::memset(pixels, 0xFF, sizeof(pixels));

        BmpBuilder bmp{alloc};
        auto span = MakeBmp(bmp, 2, 2, pixels);

        nectar::TextureImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(5)};

        // srgb=false
        nectar::HiveDocument settings{alloc};
        settings.SetValue("import", "srgb", nectar::HiveValue::MakeBool(false));
        settings.SetValue("import", "generate_mipmaps",
                          nectar::HiveValue::MakeBool(false));

        auto result = importer.Import(span, settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NtexHeader*>(result.intermediate_data.Data());
        larvae::AssertFalse(header->srgb);
    });

    auto t6 = larvae::RegisterTest("NectarTexture", "InvalidData", []() {
        auto& alloc = GetTexAlloc();
        const uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF};

        nectar::TextureImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(6)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(wax::ByteSpan{garbage, sizeof(garbage)}, settings, ctx);
        larvae::AssertFalse(result.success);
        larvae::AssertTrue(result.error_message.View().Size() > 0);
    });

    auto t7 = larvae::RegisterTest("NectarTexture", "NtexHeaderValid", []() {
        auto& alloc = GetTexAlloc();
        uint8_t pixels[4 * 4 * 4];
        std::memset(pixels, 0xAA, sizeof(pixels));

        BmpBuilder bmp{alloc};
        auto span = MakeBmp(bmp, 4, 4, pixels);

        nectar::TextureImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(7)};
        nectar::HiveDocument settings{alloc};
        settings.SetValue("import", "generate_mipmaps",
                          nectar::HiveValue::MakeBool(false));

        auto result = importer.Import(span, settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NtexHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->magic, nectar::kNtexMagic);
        larvae::AssertEqual(header->version, uint32_t{1});
        larvae::AssertTrue(header->format == nectar::PixelFormat::RGBA8);
        larvae::AssertTrue(header->srgb);  // default
    });

    auto t8 = larvae::RegisterTest("NectarTexture", "Extensions", []() {
        nectar::TextureImporter importer;
        auto exts = importer.SourceExtensions();
        larvae::AssertEqual(exts.Size(), size_t{6});
        larvae::AssertTrue(wax::StringView{exts[0]}.Equals(".png"));
        larvae::AssertTrue(wax::StringView{exts[1]}.Equals(".jpg"));
    });

    auto t9 = larvae::RegisterTest("NectarTexture", "VersionAndTypeName", []() {
        nectar::TextureImporter importer;
        larvae::AssertEqual(importer.Version(), uint32_t{2});
        larvae::AssertTrue(importer.TypeName().Equals("Texture"));
    });

}
