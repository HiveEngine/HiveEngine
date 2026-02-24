#pragma once

#include <cstdint>
#include <cstddef>

namespace nectar
{
    // NMSH intermediate format (Nectar Mesh)
    // Layout in memory/file:
    //   NmshHeader
    //   SubMesh[submesh_count]
    //   MeshVertex[vertex_count]   (interleaved)
    //   uint32_t[index_count]      (indices)

    static constexpr uint32_t kNmshMagic = 0x48534D4E;  // "NMSH" little-endian

    struct MeshVertex
    {
        float position[3];
        float normal[3];
        float uv[2];
        uint32_t color;  // packed RGBA8 (R in low byte)
    };

    struct SubMesh
    {
        uint32_t index_offset{0};   // first index in the index buffer
        uint32_t index_count{0};
        int32_t  material_index{-1};
        float    aabb_min[3]{};
        float    aabb_max[3]{};
    };

    struct NmshHeader
    {
        uint32_t magic{kNmshMagic};
        uint32_t version{2};
        uint32_t vertex_count{0};
        uint32_t index_count{0};
        uint32_t submesh_count{0};
        float    aabb_min[3]{};
        float    aabb_max[3]{};
        uint32_t padding{0};       // align to 48 bytes
    };

    static_assert(sizeof(MeshVertex) == 36, "MeshVertex must be 36 bytes");
    static_assert(sizeof(SubMesh) == 36, "SubMesh must be 36 bytes");

    [[nodiscard]] constexpr size_t NmshVertexDataOffset(const NmshHeader& h) noexcept
    {
        return sizeof(NmshHeader) + sizeof(SubMesh) * h.submesh_count;
    }

    [[nodiscard]] constexpr size_t NmshIndexDataOffset(const NmshHeader& h) noexcept
    {
        return NmshVertexDataOffset(h) + sizeof(MeshVertex) * h.vertex_count;
    }

    [[nodiscard]] constexpr size_t NmshTotalSize(const NmshHeader& h) noexcept
    {
        return NmshIndexDataOffset(h) + sizeof(uint32_t) * h.index_count;
    }
}
