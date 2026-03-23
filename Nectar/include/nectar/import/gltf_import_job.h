#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

#include <cstdint>

namespace nectar
{
    class AssetDatabase;
    class ImportPipeline;

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

    using GltfProgressFn = void (*)(const char* step, uint32_t current, uint32_t total, void* userData);

    HIVE_API GltfImportResult ExecuteGltfImport(const GltfImportDesc& desc, AssetDatabase& db, comb::DefaultAllocator& alloc,
                                       ImportPipeline* pipeline = nullptr, GltfProgressFn progress = nullptr,
                                       void* progressUserData = nullptr);
} // namespace nectar
