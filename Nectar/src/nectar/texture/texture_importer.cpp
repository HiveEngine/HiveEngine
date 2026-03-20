#include <nectar/texture/texture_importer.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <cstring>
#include <stb_image.h>

namespace nectar
{
    namespace
    {
        void DownscaleHalf(const uint8_t* src, uint32_t srcW, uint32_t srcH, uint8_t* dst, uint32_t channels)
        {
            const uint32_t dstW = srcW / 2;
            const uint32_t dstH = srcH / 2;
            for (uint32_t y = 0; y < dstH; ++y)
            {
                for (uint32_t x = 0; x < dstW; ++x)
                {
                    for (uint32_t c = 0; c < channels; ++c)
                    {
                        uint32_t sum = 0;
                        sum += src[((y * 2) * srcW + (x * 2)) * channels + c];
                        sum += src[((y * 2) * srcW + (x * 2 + 1)) * channels + c];
                        sum += src[((y * 2 + 1) * srcW + (x * 2)) * channels + c];
                        sum += src[((y * 2 + 1) * srcW + (x * 2 + 1)) * channels + c];
                        dst[(y * dstW + x) * channels + c] = static_cast<uint8_t>(sum / 4);
                    }
                }
            }
        }

        void FlipVertical(uint8_t* data, uint32_t width, uint32_t height, uint32_t channels)
        {
            const uint32_t rowBytes = width * channels;
            for (uint32_t y = 0; y < height / 2; ++y)
            {
                uint8_t* rowA = data + y * rowBytes;
                uint8_t* rowB = data + (height - 1 - y) * rowBytes;
                for (uint32_t i = 0; i < rowBytes; ++i)
                {
                    const uint8_t tmp = rowA[i];
                    rowA[i] = rowB[i];
                    rowB[i] = tmp;
                }
            }
        }
    } // namespace

    wax::Span<const char* const> TextureImporter::SourceExtensions() const
    {
        static const char* const kExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr"};
        return {kExtensions, 6};
    }

    uint32_t TextureImporter::Version() const
    {
        return 2;
    }

    wax::StringView TextureImporter::TypeName() const
    {
        return "Texture";
    }

    ImportResult TextureImporter::Import(wax::ByteSpan sourceData, const HiveDocument& settings,
                                         ImportContext& context)
    {
        ImportResult result{};

        int widthInt = 0;
        int heightInt = 0;
        int originalChannels = 0;
        uint8_t* pixels = stbi_load_from_memory(sourceData.Data(), static_cast<int>(sourceData.Size()), &widthInt,
                                                &heightInt, &originalChannels, 4);

        if (pixels == nullptr || widthInt <= 0 || heightInt <= 0)
        {
            if (pixels != nullptr)
            {
                stbi_image_free(pixels);
            }
            result.m_errorMessage = wax::String{"Failed to decode image"};
            return result;
        }

        uint32_t width = static_cast<uint32_t>(widthInt);
        uint32_t height = static_cast<uint32_t>(heightInt);
        constexpr uint32_t kChannels = 4;

        const bool srgb = settings.GetBool("import", "srgb", true);
        const bool genMipmaps = settings.GetBool("import", "generate_mipmaps", true);
        const bool flipY = settings.GetBool("import", "flip_y", false);
        const int64_t maxSize = settings.GetInt("import", "max_size", 0);

        if (flipY)
        {
            FlipVertical(pixels, width, height, kChannels);
        }

        uint8_t* current = pixels;
        uint32_t currentWidth = width;
        uint32_t currentHeight = height;
        uint8_t* downscaled = nullptr;

        if (maxSize > 0)
        {
            while (currentWidth > static_cast<uint32_t>(maxSize) || currentHeight > static_cast<uint32_t>(maxSize))
            {
                if (currentWidth < 2 || currentHeight < 2)
                {
                    break;
                }

                const uint32_t newWidth = currentWidth / 2;
                const uint32_t newHeight = currentHeight / 2;
                const size_t dstSize = static_cast<size_t>(newWidth) * newHeight * kChannels;
                auto* dst = static_cast<uint8_t*>(context.GetAllocator().Allocate(dstSize, 1));
                if (dst == nullptr)
                {
                    stbi_image_free(pixels);
                    if (downscaled != nullptr)
                    {
                        context.GetAllocator().Deallocate(downscaled);
                    }
                    result.m_errorMessage = wax::String{"Failed to allocate downscale buffer"};
                    return result;
                }

                DownscaleHalf(current, currentWidth, currentHeight, dst, kChannels);

                if (downscaled != nullptr)
                {
                    context.GetAllocator().Deallocate(downscaled);
                }
                downscaled = dst;
                current = dst;
                currentWidth = newWidth;
                currentHeight = newHeight;
            }
        }

        width = currentWidth;
        height = currentHeight;

        uint8_t mipCount = 1;
        if (genMipmaps)
        {
            uint32_t mipWidth = width;
            uint32_t mipHeight = height;
            while (mipWidth > 1 || mipHeight > 1)
            {
                mipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
                mipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;
                ++mipCount;
            }
        }

        NtexHeader header{};
        header.m_width = width;
        header.m_height = height;
        header.m_channels = kChannels;
        header.m_format = PixelFormat::RGBA8;
        header.m_srgb = srgb;
        header.m_mipCount = mipCount;

        size_t totalPixelBytes = 0;
        {
            uint32_t mipWidth = width;
            uint32_t mipHeight = height;
            for (uint8_t i = 0; i < mipCount; ++i)
            {
                totalPixelBytes += static_cast<size_t>(mipWidth) * mipHeight * kChannels;
                mipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
                mipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;
            }
        }

        const size_t headerSize = sizeof(NtexHeader);
        const size_t mipTableSize = sizeof(TextureMipLevel) * mipCount;
        const size_t totalSize = headerSize + mipTableSize + totalPixelBytes;

        result.m_intermediateData.Resize(totalSize);
        uint8_t* blob = result.m_intermediateData.Data();

        std::memcpy(blob, &header, headerSize);
        size_t offset = headerSize;

        auto* mipTable = reinterpret_cast<TextureMipLevel*>(blob + offset);
        offset += mipTableSize;

        uint32_t mipDataOffset = 0;
        const size_t baseSize = static_cast<size_t>(width) * height * kChannels;
        mipTable[0] = TextureMipLevel{width, height, 0, static_cast<uint32_t>(baseSize)};
        std::memcpy(blob + offset, current, baseSize);
        mipDataOffset += static_cast<uint32_t>(baseSize);

        if (mipCount > 1)
        {
            const uint8_t* prev = current;
            uint32_t prevWidth = width;
            uint32_t prevHeight = height;

            for (uint8_t mipIndex = 1; mipIndex < mipCount; ++mipIndex)
            {
                const uint32_t newWidth = (prevWidth > 1) ? prevWidth / 2 : 1;
                const uint32_t newHeight = (prevHeight > 1) ? prevHeight / 2 : 1;
                const size_t mipSize = static_cast<size_t>(newWidth) * newHeight * kChannels;

                uint8_t* mipDst = blob + offset + mipDataOffset;
                DownscaleHalf(prev, prevWidth, prevHeight, mipDst, kChannels);

                mipTable[mipIndex] =
                    TextureMipLevel{newWidth, newHeight, mipDataOffset, static_cast<uint32_t>(mipSize)};
                mipDataOffset += static_cast<uint32_t>(mipSize);

                prev = mipDst;
                prevWidth = newWidth;
                prevHeight = newHeight;
            }
        }

        stbi_image_free(pixels);
        if (downscaled != nullptr)
        {
            context.GetAllocator().Deallocate(downscaled);
        }

        result.m_success = true;
        return result;
    }
} // namespace nectar
