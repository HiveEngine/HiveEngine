#pragma once

#include <nectar/server/asset_loader.h>
#include <nectar/mesh/mesh_data.h>
#include <comb/new.h>
#include <cstring>

namespace nectar
{

struct MeshAsset
{
    NmshHeader header{};
    uint8_t* data{nullptr};
    size_t data_size{0};

    [[nodiscard]] const SubMesh* Submeshes() const
    {
        return reinterpret_cast<const SubMesh*>(data + sizeof(NmshHeader));
    }

    [[nodiscard]] const MeshVertex* Vertices() const
    {
        return reinterpret_cast<const MeshVertex*>(
            data + NmshVertexDataOffset(header));
    }

    [[nodiscard]] const uint32_t* Indices() const
    {
        return reinterpret_cast<const uint32_t*>(
            data + NmshIndexDataOffset(header));
    }
};

class MeshAssetLoader final : public AssetLoader<MeshAsset>
{
public:
    [[nodiscard]] MeshAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
    {
        if (data.Size() < sizeof(NmshHeader))
            return nullptr;

        auto* raw_header = reinterpret_cast<const NmshHeader*>(data.Data());
        if (raw_header->magic != kNmshMagic)
            return nullptr;

        size_t expected = NmshTotalSize(*raw_header);
        if (data.Size() < expected)
            return nullptr;

        auto* blob = static_cast<uint8_t*>(alloc.Allocate(expected, alignof(std::max_align_t)));
        if (!blob) return nullptr;

        auto* asset = comb::New<MeshAsset>(alloc);
        asset->header = *raw_header;
        asset->data_size = expected;
        asset->data = blob;
        std::memcpy(blob, data.Data(), expected);

        return asset;
    }

    void Unload(MeshAsset* asset, comb::DefaultAllocator& alloc) override
    {
        if (asset)
        {
            if (asset->data)
                alloc.Deallocate(asset->data);
            comb::Delete(alloc, asset);
        }
    }

    [[nodiscard]] size_t SizeOf(const MeshAsset* asset) const override
    {
        return sizeof(MeshAsset) + (asset ? asset->data_size : 0);
    }
};

} // namespace nectar
