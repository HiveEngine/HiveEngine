#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_span.h>

namespace nectar
{
    /// Material info extracted from a glTF file.
    struct GltfMaterialInfo
    {
        int32_t m_materialIndex{-1};
        wax::String m_name{};
        wax::String m_baseColorTexture{}; // relative path to albedo texture (empty if none)
        float m_baseColorFactor[4]{1.f, 1.f, 1.f, 1.f};
        wax::String m_normalTexture{};
        wax::String m_metallicRoughnessTexture{};
        float m_metallicFactor{1.f};
        float m_roughnessFactor{1.f};
        float m_alphaCutoff{0.f}; // >0 enables alpha test (glTF MASK mode)
        bool m_doubleSided{false};
    };

    /// Parse a glTF/GLB blob and extract per-material texture info.
    /// Returns one entry per material in the glTF materials[] array.
    wax::Vector<GltfMaterialInfo> ParseGltfMaterials(wax::ByteSpan gltfData, comb::DefaultAllocator& alloc);
} // namespace nectar
