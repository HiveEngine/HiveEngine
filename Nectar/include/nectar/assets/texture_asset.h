#pragma once

#include <comb/new.h>

#include <nectar/server/asset_loader.h>
#include <nectar/texture/texture_data.h>

#include <cstring>

namespace nectar
{

    struct TextureAsset
    {
        NtexHeader header{};
        uint8_t* data{nullptr};
        size_t data_size{0};

        [[nodiscard]] const TextureMipLevel* MipLevels() const
        {
            return reinterpret_cast<const TextureMipLevel*>(data + sizeof(NtexHeader));
        }

        [[nodiscard]] const uint8_t* PixelData() const
        {
            return data + sizeof(NtexHeader) + sizeof(TextureMipLevel) * header.m_mipCount;
        }

        [[nodiscard]] const uint8_t* MipData(uint8_t level) const
        {
            auto* mips = MipLevels();
            return PixelData() + mips[level].m_offset;
        }
    };

    class TextureAssetLoader final : public AssetLoader<TextureAsset>
    {
    public:
        [[nodiscard]] TextureAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            if (data.Size() < sizeof(NtexHeader))
                return nullptr;

            NtexHeader hdr{};
            std::memcpy(&hdr, data.Data(), sizeof(NtexHeader));
            if (hdr.m_magic != kNtexMagic)
                return nullptr;

            auto* blob = static_cast<uint8_t*>(alloc.Allocate(data.Size(), alignof(std::max_align_t)));
            if (!blob)
                return nullptr;

            auto* asset = comb::New<TextureAsset>(alloc);
            asset->header = hdr;
            asset->data_size = data.Size();
            asset->data = blob;
            std::memcpy(blob, data.Data(), data.Size());

            return asset;
        }

        void Unload(TextureAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset)
            {
                if (asset->data)
                    alloc.Deallocate(asset->data);
                comb::Delete(alloc, asset);
            }
        }

        [[nodiscard]] size_t SizeOf(const TextureAsset* asset) const override
        {
            return sizeof(TextureAsset) + (asset ? asset->data_size : 0);
        }
    };

} // namespace nectar
