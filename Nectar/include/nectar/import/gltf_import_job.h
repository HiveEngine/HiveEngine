#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

#include <cstdint>

namespace nectar
{
    class AssetDatabase;

    struct GltfImportDesc
    {
        wax::StringView m_gltfPath;
        wax::StringView m_assetsDir;
    };

    struct GltfImportResult
    {
        bool m_success{false};
        uint32_t m_textureCount{0};
        uint32_t m_materialCount{0};
        uint32_t m_meshCount{0};
        wax::String m_error;
    };

    GltfImportResult ExecuteGltfImport(const GltfImportDesc& desc, AssetDatabase& db, comb::DefaultAllocator& alloc);
} // namespace nectar
