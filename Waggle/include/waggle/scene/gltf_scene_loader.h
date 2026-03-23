#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>

#include <queen/core/entity.h>

#include <cstdint>

namespace queen
{
    class World;
}

namespace waggle
{

    struct GltfSceneLoadResult
    {
        uint32_t m_entityCount{0};
        uint32_t m_meshCount{0};
        uint32_t m_cameraCount{0};
        uint32_t m_lightCount{0};
        bool m_success{false};
    };

    HIVE_API GltfSceneLoadResult LoadGltfScene(queen::World& world, const void* data, size_t dataSize, wax::StringView basePath,
                                               comb::DefaultAllocator& alloc);

} // namespace waggle
