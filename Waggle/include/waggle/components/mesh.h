#pragma once

#include <hive/HiveConfig.h>

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
#include <swarm/types.h>
#endif

#include <cstdint>

namespace waggle
{

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
struct MeshRenderable
{
    swarm::BufferHandle vertex_buffer{};
    swarm::BufferHandle index_buffer{};
    uint32_t            index_count{0};
    uint32_t            index_offset{0};
    int32_t             material_index{-1};
};
#endif

} // namespace waggle
