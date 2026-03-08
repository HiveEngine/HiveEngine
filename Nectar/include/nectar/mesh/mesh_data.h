#pragma once

#include <cstddef>
#include <cstdint>

namespace nectar
{
    // NMSH intermediate format (Nectar Mesh)
    // Layout in memory/file:
    //   NmshHeader
    //   SubMesh[submesh_count]
    //   MeshVertex[vertex_count]   (interleaved)
    //   uint32_t[index_count]      (indices)

    static constexpr uint32_t kNmshMagic = 0x48534D4E; // "NMSH" little-endian

    struct MeshVertex
    {
        float m_position[3];
        float m_normal[3];
        float m_uv[2];
        uint32_t m_color; // packed RGBA8 (R in low byte)
    };

    struct SubMesh
    {
        uint32_t m_indexOffset{0}; // first index in the index buffer
        uint32_t m_indexCount{0};
        int32_t m_materialIndex{-1};
        float m_aabbMin[3]{};
        float m_aabbMax[3]{};
    };

    struct NmshHeader
    {
        uint32_t m_magic{kNmshMagic};
        uint32_t m_version{2};
        uint32_t m_vertexCount{0};
        uint32_t m_indexCount{0};
        uint32_t m_submeshCount{0};
        float m_aabbMin[3]{};
        float m_aabbMax[3]{};
        uint32_t m_padding{0}; // align to 48 bytes
    };

    static_assert(sizeof(MeshVertex) == 36, "MeshVertex must be 36 bytes");
    static_assert(sizeof(SubMesh) == 36, "SubMesh must be 36 bytes");

    [[nodiscard]] constexpr size_t NmshVertexDataOffset(const NmshHeader& h) noexcept
    {
        return sizeof(NmshHeader) + sizeof(SubMesh) * h.m_submeshCount;
    }

    [[nodiscard]] constexpr size_t NmshIndexDataOffset(const NmshHeader& h) noexcept
    {
        return NmshVertexDataOffset(h) + sizeof(MeshVertex) * h.m_vertexCount;
    }

    [[nodiscard]] constexpr size_t NmshTotalSize(const NmshHeader& h) noexcept
    {
        return NmshIndexDataOffset(h) + sizeof(uint32_t) * h.m_indexCount;
    }
} // namespace nectar
