#pragma once

#include <hive/core/assert.h>

#include <cstddef>
#include <cstdint>
#include <limits>

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
        float m_tangent[4]; // xyz = tangent direction, w = bitangent sign (+1 or -1)
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
        uint32_t m_version{3};
        uint32_t m_vertexCount{0};
        uint32_t m_indexCount{0};
        uint32_t m_submeshCount{0};
        float m_aabbMin[3]{};
        float m_aabbMax[3]{};
        uint32_t m_padding{0}; // align to 48 bytes
    };

    static_assert(sizeof(MeshVertex) == 52, "MeshVertex must be 52 bytes");
    static_assert(sizeof(SubMesh) == 36, "SubMesh must be 36 bytes");

    // 256 MB — sane upper bound for a single mesh blob in memory
    static constexpr size_t kNmshMaxBytes = 256u * 1024u * 1024u;

    [[nodiscard]] constexpr size_t NmshVertexDataOffset(const NmshHeader& h)
    {
        const size_t submeshBytes = static_cast<size_t>(h.m_submeshCount) * sizeof(SubMesh);
        const size_t offset = sizeof(NmshHeader) + submeshBytes;
        hive::Check(offset >= sizeof(NmshHeader), "NmshVertexDataOffset: submesh size overflow");
        return offset;
    }

    [[nodiscard]] constexpr size_t NmshIndexDataOffset(const NmshHeader& h)
    {
        const size_t vertexOffset = NmshVertexDataOffset(h);
        const size_t vertexBytes = static_cast<size_t>(h.m_vertexCount) * sizeof(MeshVertex);
        const size_t offset = vertexOffset + vertexBytes;
        hive::Check(offset >= vertexOffset, "NmshIndexDataOffset: vertex size overflow");
        return offset;
    }

    [[nodiscard]] constexpr size_t NmshTotalSize(const NmshHeader& h)
    {
        const size_t indexOffset = NmshIndexDataOffset(h);
        const size_t indexBytes = static_cast<size_t>(h.m_indexCount) * sizeof(uint32_t);
        const size_t total = indexOffset + indexBytes;
        hive::Check(total >= indexOffset, "NmshTotalSize: index size overflow");
        hive::Check(total <= kNmshMaxBytes, "NmshTotalSize: mesh exceeds 256 MB limit");
        return total;
    }
} // namespace nectar
