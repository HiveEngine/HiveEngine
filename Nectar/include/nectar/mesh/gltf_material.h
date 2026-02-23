#pragma once

#include <wax/serialization/byte_span.h>
#include <wax/containers/string.h>
#include <wax/containers/vector.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Material info extracted from a glTF file.
    struct GltfMaterialInfo
    {
        int32_t material_index{-1};
        wax::String<> base_color_texture{};       // relative path to albedo texture (empty if none)
        float base_color_factor[4]{1.f, 1.f, 1.f, 1.f};
        wax::String<> normal_texture{};
        wax::String<> metallic_roughness_texture{};
        float metallic_factor{1.f};
        float roughness_factor{1.f};
        float alpha_cutoff{0.f};   // >0 enables alpha test (glTF MASK mode)
        bool  double_sided{false};
    };

    /// Parse a glTF/GLB blob and extract per-material texture info.
    /// Returns one entry per material in the glTF materials[] array.
    wax::Vector<GltfMaterialInfo> ParseGltfMaterials(wax::ByteSpan gltf_data,
                                                      comb::DefaultAllocator& alloc);
}
