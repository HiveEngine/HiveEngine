#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

#include <nectar/texture/texture_importer.h>
#include <nectar/hive/hive_document.h>
#include <cstring>

namespace nectar
{
    wax::Span<const char* const> TextureImporter::SourceExtensions() const
    {
        static const char* const exts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr"};
        return {exts, 6};
    }

    uint32_t TextureImporter::Version() const { return 2; }

    wax::StringView TextureImporter::TypeName() const { return "Texture"; }

    // Box-filter downscale by half (RGBA)
    static void DownscaleHalf(const uint8_t* src, uint32_t src_w, uint32_t src_h,
                               uint8_t* dst, uint32_t channels)
    {
        uint32_t dst_w = src_w / 2;
        uint32_t dst_h = src_h / 2;
        for (uint32_t y = 0; y < dst_h; ++y)
        {
            for (uint32_t x = 0; x < dst_w; ++x)
            {
                for (uint32_t c = 0; c < channels; ++c)
                {
                    uint32_t sum = 0;
                    sum += src[((y * 2) * src_w + (x * 2)) * channels + c];
                    sum += src[((y * 2) * src_w + (x * 2 + 1)) * channels + c];
                    sum += src[((y * 2 + 1) * src_w + (x * 2)) * channels + c];
                    sum += src[((y * 2 + 1) * src_w + (x * 2 + 1)) * channels + c];
                    dst[(y * dst_w + x) * channels + c] = static_cast<uint8_t>(sum / 4);
                }
            }
        }
    }

    static void FlipVertical(uint8_t* data, uint32_t w, uint32_t h, uint32_t channels)
    {
        uint32_t row_bytes = w * channels;
        for (uint32_t y = 0; y < h / 2; ++y)
        {
            uint8_t* row_a = data + y * row_bytes;
            uint8_t* row_b = data + (h - 1 - y) * row_bytes;
            for (uint32_t i = 0; i < row_bytes; ++i)
            {
                uint8_t tmp = row_a[i];
                row_a[i] = row_b[i];
                row_b[i] = tmp;
            }
        }
    }

    ImportResult TextureImporter::Import(wax::ByteSpan source_data,
                                          const HiveDocument& settings,
                                          ImportContext& /*context*/)
    {
        ImportResult result{};

        // Decode with stb_image — force 4 channels (RGBA)
        int w = 0, h = 0, original_channels = 0;
        uint8_t* pixels = stbi_load_from_memory(
            source_data.Data(), static_cast<int>(source_data.Size()),
            &w, &h, &original_channels, 4);

        if (!pixels || w <= 0 || h <= 0)
        {
            if (pixels) stbi_image_free(pixels);
            result.error_message = wax::String<>{"Failed to decode image"};
            return result;
        }

        uint32_t width = static_cast<uint32_t>(w);
        uint32_t height = static_cast<uint32_t>(h);
        constexpr uint32_t channels = 4;

        // Read settings
        bool srgb = settings.GetBool("import", "srgb", true);
        bool gen_mipmaps = settings.GetBool("import", "generate_mipmaps", true);
        bool flip_y = settings.GetBool("import", "flip_y", false);
        int64_t max_size = settings.GetInt("import", "max_size", 0);

        // Flip Y
        if (flip_y)
            FlipVertical(pixels, width, height, channels);

        // Downscale to max_size (halve until within bounds)
        uint8_t* current = pixels;
        uint32_t cur_w = width, cur_h = height;
        uint8_t* downscaled = nullptr;

        if (max_size > 0)
        {
            while (cur_w > static_cast<uint32_t>(max_size) ||
                   cur_h > static_cast<uint32_t>(max_size))
            {
                if (cur_w < 2 || cur_h < 2) break;

                uint32_t new_w = cur_w / 2;
                uint32_t new_h = cur_h / 2;
                auto* dst = static_cast<uint8_t*>(std::malloc(new_w * new_h * channels));
                DownscaleHalf(current, cur_w, cur_h, dst, channels);

                if (downscaled) std::free(downscaled);
                downscaled = dst;
                current = dst;
                cur_w = new_w;
                cur_h = new_h;
            }
        }

        width = cur_w;
        height = cur_h;

        // Count mip levels
        uint8_t mip_count = 1;
        if (gen_mipmaps)
        {
            uint32_t mw = width, mh = height;
            while (mw > 1 || mh > 1)
            {
                mw = (mw > 1) ? mw / 2 : 1;
                mh = (mh > 1) ? mh / 2 : 1;
                ++mip_count;
            }
        }

        // Build NTEX blob
        NtexHeader header{};
        header.width = width;
        header.height = height;
        header.channels = channels;
        header.format = PixelFormat::RGBA8;
        header.srgb = srgb;
        header.mip_count = mip_count;

        // Compute total pixel data size
        size_t total_pixel_bytes = 0;
        {
            uint32_t mw = width, mh = height;
            for (uint8_t i = 0; i < mip_count; ++i)
            {
                total_pixel_bytes += mw * mh * channels;
                mw = (mw > 1) ? mw / 2 : 1;
                mh = (mh > 1) ? mh / 2 : 1;
            }
        }

        size_t header_size = sizeof(NtexHeader);
        size_t mip_table_size = sizeof(TextureMipLevel) * mip_count;
        size_t total_size = header_size + mip_table_size + total_pixel_bytes;

        result.intermediate_data.Resize(total_size);
        uint8_t* blob = result.intermediate_data.Data();

        // Write header
        std::memcpy(blob, &header, header_size);
        size_t offset = header_size;

        // Build mip table and pixel data
        auto* mip_table = reinterpret_cast<TextureMipLevel*>(blob + offset);
        offset += mip_table_size;

        // Mip 0 = the (potentially downscaled) base image
        uint32_t mip_data_offset = 0;
        size_t base_size = static_cast<size_t>(width) * height * channels;
        mip_table[0] = TextureMipLevel{width, height, 0, static_cast<uint32_t>(base_size)};
        std::memcpy(blob + offset, current, base_size);

        mip_data_offset += static_cast<uint32_t>(base_size);

        // Generate remaining mip levels
        if (mip_count > 1)
        {
            const uint8_t* prev = current;
            uint32_t prev_w = width, prev_h = height;

            for (uint8_t m = 1; m < mip_count; ++m)
            {
                uint32_t new_w = (prev_w > 1) ? prev_w / 2 : 1;
                uint32_t new_h = (prev_h > 1) ? prev_h / 2 : 1;
                size_t mip_size = static_cast<size_t>(new_w) * new_h * channels;

                uint8_t* mip_dst = blob + offset + mip_data_offset;
                DownscaleHalf(prev, prev_w, prev_h, mip_dst, channels);

                mip_table[m] = TextureMipLevel{new_w, new_h, mip_data_offset,
                                                static_cast<uint32_t>(mip_size)};
                mip_data_offset += static_cast<uint32_t>(mip_size);

                prev = mip_dst;
                prev_w = new_w;
                prev_h = new_h;
            }
        }

        stbi_image_free(pixels);
        if (downscaled) std::free(downscaled);

        result.success = true;
        return result;
    }
}
