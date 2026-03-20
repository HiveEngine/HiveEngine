#define CGLTF_IMPLEMENTATION

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wcast-align"
#include <cgltf.h>
#pragma clang diagnostic pop

#include <nectar/mesh/gltf_importer.h>

#include <wax/containers/string.h>
#include <wax/containers/vector.h>

#include <nectar/hive/hive_document.h>
#include <nectar/pipeline/import_context.h>

#include <wax/containers/hash_map.h>

#include <cmath>
#include <cstring>

namespace nectar
{
    wax::Span<const char* const> GltfImporter::SourceExtensions() const
    {
        static const char* const EXTS[] = {".gltf", ".glb"};
        return {EXTS, 2};
    }

    uint32_t GltfImporter::Version() const
    {
        return 3;
    }

    wax::StringView GltfImporter::TypeName() const
    {
        return "Mesh";
    }

    static uint32_t PackRGBA8(float r, float g, float b, float a = 1.f)
    {
        auto toU8 = [](float v) -> uint32_t {
            return static_cast<uint32_t>(v * 255.f + 0.5f) & 0xFFu;
        };
        return toU8(r) | (toU8(g) << 8) | (toU8(b) << 16) | (toU8(a) << 24);
    }

    ImportResult GltfImporter::Import(wax::ByteSpan sourceData, const HiveDocument& settings, ImportContext& context)
    {
        ImportResult result{};

        auto scale = static_cast<float>(settings.GetFloat("import", "scale", 1.0));
        bool flipUv = settings.GetBool("import", "flip_uv", false);
        bool genNormals = settings.GetBool("import", "generate_normals", true);
        bool genTangents = settings.GetBool("import", "generate_tangents", true);
        auto basePath = settings.GetString("import", "base_path", "");

        // Parse glTF/GLB from memory
        cgltf_options options{};
        cgltf_data* data = nullptr;
        cgltf_result res = cgltf_parse(&options, sourceData.Data(), sourceData.Size(), &data);
        if (res != cgltf_result_success)
        {
            result.m_errorMessage = wax::String{"glTF parse failed"};
            return result;
        }

        // Load buffers: external .bin for .gltf, embedded for .glb
        if (!basePath.IsEmpty())
        {
            wax::String path{basePath};
            res = cgltf_load_buffers(&options, data, path.CStr());
        }
        else
        {
            res = cgltf_load_buffers(&options, data, nullptr);
        }
        if (res != cgltf_result_success)
        {
            cgltf_free(data);
            result.m_errorMessage = wax::String{"glTF buffer load failed"};
            return result;
        }

        // Collect geometry from all meshes/primitives
        auto& alloc = context.GetAllocator();
        wax::Vector<MeshVertex> vertices{alloc};
        wax::HashMap<int, wax::Vector<uint32_t>> matIndices;

        float aabbMin[3] = {1e30f, 1e30f, 1e30f};
        float aabbMax[3] = {-1e30f, -1e30f, -1e30f};

        for (cgltf_size mi = 0; mi < data->meshes_count; ++mi)
        {
            const auto& mesh = data->meshes[mi];

            for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi)
            {
                const auto& prim = mesh.primitives[pi];

                // Only triangles
                if (prim.type != cgltf_primitive_type_triangles)
                    continue;

                // Find attribute accessors
                const cgltf_accessor* posAcc = nullptr;
                const cgltf_accessor* normAcc = nullptr;
                const cgltf_accessor* uvAcc = nullptr;
                const cgltf_accessor* colorAcc = nullptr;
                const cgltf_accessor* tanAcc = nullptr;

                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
                {
                    const auto& attr = prim.attributes[ai];
                    if (attr.type == cgltf_attribute_type_position)
                        posAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_normal)
                        normAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0)
                        uvAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_color && attr.index == 0)
                        colorAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_tangent)
                        tanAcc = attr.data;
                }

                if (!posAcc)
                    continue;

                // Material index (offset in data->materials array)
                int matId = -1;
                if (prim.material)
                    matId = static_cast<int>(prim.material - data->materials);

                // Base color factor from PBR material
                float baseColor[4] = {1.f, 1.f, 1.f, 1.f};
                if (prim.material && prim.material->has_pbr_metallic_roughness)
                    std::memcpy(baseColor, prim.material->pbr_metallic_roughness.base_color_factor, 4 * sizeof(float));

                uint32_t baseVertex = static_cast<uint32_t>(vertices.Size());
                auto vertCount = static_cast<cgltf_size>(posAcc->count);

                // Extract vertices
                for (cgltf_size vi = 0; vi < vertCount; ++vi)
                {
                    MeshVertex vert{};

                    // Position
                    float pos[3] = {};
                    cgltf_accessor_read_float(posAcc, vi, pos, 3);
                    vert.m_position[0] = pos[0] * scale;
                    vert.m_position[1] = pos[1] * scale;
                    vert.m_position[2] = pos[2] * scale;

                    for (int a = 0; a < 3; ++a)
                    {
                        if (vert.m_position[a] < aabbMin[a])
                            aabbMin[a] = vert.m_position[a];
                        if (vert.m_position[a] > aabbMax[a])
                            aabbMax[a] = vert.m_position[a];
                    }

                    // Normal
                    if (normAcc)
                    {
                        float norm[3] = {};
                        cgltf_accessor_read_float(normAcc, vi, norm, 3);
                        vert.m_normal[0] = norm[0];
                        vert.m_normal[1] = norm[1];
                        vert.m_normal[2] = norm[2];
                    }

                    // UV
                    if (uvAcc)
                    {
                        float uv[2] = {};
                        cgltf_accessor_read_float(uvAcc, vi, uv, 2);
                        vert.m_uv[0] = uv[0];
                        vert.m_uv[1] = flipUv ? 1.0f - uv[1] : uv[1];
                    }

                    // Vertex color from attribute, or material base color
                    if (colorAcc)
                    {
                        float col[4] = {1.f, 1.f, 1.f, 1.f};
                        cgltf_accessor_read_float(colorAcc, vi, col, 4);
                        vert.m_color = PackRGBA8(col[0], col[1], col[2], col[3]);
                    }
                    else
                    {
                        vert.m_color = PackRGBA8(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
                    }

                    if (tanAcc)
                    {
                        cgltf_accessor_read_float(tanAcc, vi, vert.m_tangent, 4);
                    }

                    vertices.PushBack(vert);
                }

                // Extract indices
                auto& idxBuf = matIndices[matId];
                if (prim.indices)
                {
                    for (cgltf_size ii = 0; ii < prim.indices->count; ++ii)
                    {
                        auto idx = cgltf_accessor_read_index(prim.indices, ii);
                        idxBuf.PushBack(baseVertex + static_cast<uint32_t>(idx));
                    }
                }
                else
                {
                    for (uint32_t vi = 0; vi < static_cast<uint32_t>(vertCount); ++vi)
                        idxBuf.PushBack(baseVertex + vi);
                }

                // Generate face normals if source has none
                if (!normAcc && genNormals)
                {
                    size_t idxEnd = idxBuf.Size();
                    size_t idxStart = idxEnd - (prim.indices ? prim.indices->count : vertCount);
                    size_t triCount = (idxEnd - idxStart) / 3;

                    for (size_t ti = 0; ti < triCount; ++ti)
                    {
                        uint32_t i0 = idxBuf[idxStart + ti * 3 + 0];
                        uint32_t i1 = idxBuf[idxStart + ti * 3 + 1];
                        uint32_t i2 = idxBuf[idxStart + ti * 3 + 2];

                        float* p0 = vertices[i0].m_position;
                        float* p1 = vertices[i1].m_position;
                        float* p2 = vertices[i2].m_position;

                        float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
                        float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

                        float n[3];
                        n[0] = e1[1] * e2[2] - e1[2] * e2[1];
                        n[1] = e1[2] * e2[0] - e1[0] * e2[2];
                        n[2] = e1[0] * e2[1] - e1[1] * e2[0];

                        float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                        if (len > 1e-8f)
                        {
                            n[0] /= len;
                            n[1] /= len;
                            n[2] /= len;
                        }

                        vertices[i0].m_normal[0] = n[0];
                        vertices[i0].m_normal[1] = n[1];
                        vertices[i0].m_normal[2] = n[2];
                        vertices[i1].m_normal[0] = n[0];
                        vertices[i1].m_normal[1] = n[1];
                        vertices[i1].m_normal[2] = n[2];
                        vertices[i2].m_normal[0] = n[0];
                        vertices[i2].m_normal[1] = n[1];
                        vertices[i2].m_normal[2] = n[2];
                    }
                }

                // Generate tangents from UV derivatives when not provided by glTF
                if (!tanAcc && genTangents && uvAcc && (normAcc || genNormals))
                {
                    size_t idxEnd = idxBuf.Size();
                    size_t idxStart = idxEnd - (prim.indices ? prim.indices->count : vertCount);
                    size_t triCount = (idxEnd - idxStart) / 3;

                    // Accumulate per-vertex tangent/bitangent
                    wax::Vector<float> tanAccum(vertices.Size() * 3);
                    tanAccum.Resize(vertices.Size() * 3);
                    std::memset(tanAccum.Data(), 0, tanAccum.Size() * sizeof(float));
                    wax::Vector<float> bitanAccum(vertices.Size() * 3);
                    bitanAccum.Resize(vertices.Size() * 3);
                    std::memset(bitanAccum.Data(), 0, bitanAccum.Size() * sizeof(float));

                    for (size_t ti = 0; ti < triCount; ++ti)
                    {
                        uint32_t i0 = idxBuf[idxStart + ti * 3 + 0];
                        uint32_t i1 = idxBuf[idxStart + ti * 3 + 1];
                        uint32_t i2 = idxBuf[idxStart + ti * 3 + 2];

                        const float* p0 = vertices[i0].m_position;
                        const float* p1 = vertices[i1].m_position;
                        const float* p2 = vertices[i2].m_position;

                        const float* uv0 = vertices[i0].m_uv;
                        const float* uv1 = vertices[i1].m_uv;
                        const float* uv2 = vertices[i2].m_uv;

                        float edge1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
                        float edge2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

                        float duv1[2] = {uv1[0] - uv0[0], uv1[1] - uv0[1]};
                        float duv2[2] = {uv2[0] - uv0[0], uv2[1] - uv0[1]};

                        float det = duv1[0] * duv2[1] - duv2[0] * duv1[1];
                        if (std::fabs(det) < 1e-8f)
                            continue;

                        float f = 1.0f / det;

                        float t[3] = {
                            f * (duv2[1] * edge1[0] - duv1[1] * edge2[0]),
                            f * (duv2[1] * edge1[1] - duv1[1] * edge2[1]),
                            f * (duv2[1] * edge1[2] - duv1[1] * edge2[2]),
                        };

                        float b[3] = {
                            f * (-duv2[0] * edge1[0] + duv1[0] * edge2[0]),
                            f * (-duv2[0] * edge1[1] + duv1[0] * edge2[1]),
                            f * (-duv2[0] * edge1[2] + duv1[0] * edge2[2]),
                        };

                        for (uint32_t vi : {i0, i1, i2})
                        {
                            for (int a = 0; a < 3; ++a)
                            {
                                tanAccum[vi * 3 + a] += t[a];
                                bitanAccum[vi * 3 + a] += b[a];
                            }
                        }
                    }

                    // Orthonormalize and store
                    for (uint32_t vi = baseVertex; vi < static_cast<uint32_t>(vertices.Size()); ++vi)
                    {
                        const float* n = vertices[vi].m_normal;
                        float* ta = &tanAccum[vi * 3];

                        // Gram-Schmidt: t = normalize(ta - n * dot(n, ta))
                        float ndot = n[0] * ta[0] + n[1] * ta[1] + n[2] * ta[2];
                        float orth[3] = {ta[0] - n[0] * ndot, ta[1] - n[1] * ndot, ta[2] - n[2] * ndot};
                        float len = std::sqrt(orth[0] * orth[0] + orth[1] * orth[1] + orth[2] * orth[2]);

                        if (len > 1e-8f)
                        {
                            vertices[vi].m_tangent[0] = orth[0] / len;
                            vertices[vi].m_tangent[1] = orth[1] / len;
                            vertices[vi].m_tangent[2] = orth[2] / len;
                        }
                        else
                        {
                            vertices[vi].m_tangent[0] = 1.f;
                            vertices[vi].m_tangent[1] = 0.f;
                            vertices[vi].m_tangent[2] = 0.f;
                        }

                        // w = sign of dot(cross(n, tangent), bitangent)
                        float* ba = &bitanAccum[vi * 3];
                        float cx[3] = {
                            n[1] * vertices[vi].m_tangent[2] - n[2] * vertices[vi].m_tangent[1],
                            n[2] * vertices[vi].m_tangent[0] - n[0] * vertices[vi].m_tangent[2],
                            n[0] * vertices[vi].m_tangent[1] - n[1] * vertices[vi].m_tangent[0],
                        };
                        float w = cx[0] * ba[0] + cx[1] * ba[1] + cx[2] * ba[2];
                        vertices[vi].m_tangent[3] = w < 0.f ? -1.f : 1.f;
                    }
                }
            }
        }

        cgltf_free(data);

        if (vertices.IsEmpty())
        {
            result.m_errorMessage = wax::String{"glTF contains no geometry"};
            return result;
        }

        // Flatten per-material buckets into final index buffer + submeshes
        wax::Vector<uint32_t> indices;
        wax::Vector<SubMesh> submeshes;

        for (auto&& [mat_id, idx_buf] : matIndices)
        {
            if (idx_buf.IsEmpty())
                continue;
            SubMesh sub{};
            sub.m_indexOffset = static_cast<uint32_t>(indices.Size());
            sub.m_indexCount = static_cast<uint32_t>(idx_buf.Size());
            sub.m_materialIndex = mat_id;
            for (size_t i = 0; i < idx_buf.Size(); ++i)
                indices.PushBack(idx_buf[i]);
            submeshes.PushBack(sub);
        }

        // Per-submesh AABB
        for (auto& sub : submeshes)
        {
            float smin[3] = {1e30f, 1e30f, 1e30f};
            float smax[3] = {-1e30f, -1e30f, -1e30f};
            for (uint32_t i = 0; i < sub.m_indexCount; ++i)
            {
                uint32_t vi = indices[sub.m_indexOffset + i];
                for (int a = 0; a < 3; ++a)
                {
                    if (vertices[vi].m_position[a] < smin[a])
                        smin[a] = vertices[vi].m_position[a];
                    if (vertices[vi].m_position[a] > smax[a])
                        smax[a] = vertices[vi].m_position[a];
                }
            }
            std::memcpy(sub.m_aabbMin, smin, sizeof(smin));
            std::memcpy(sub.m_aabbMax, smax, sizeof(smax));
        }

        // Declare BUILD dependencies on referenced materials
        auto vfsDir = settings.GetString("import", "vfs_dir", "");
        if (!vfsDir.IsEmpty() && data->materials_count > 0)
        {
            for (cgltf_size mi = 0; mi < data->materials_count; ++mi)
            {
                const char* matName = data->materials[mi].name;
                if (!matName || matName[0] == '\0')
                    continue;

                wax::String matVfsPath;
                matVfsPath.Append(vfsDir.Data(), vfsDir.Size());
                matVfsPath.Append(matName, std::strlen(matName));
                matVfsPath.Append(".hmat", 5);

                AssetId matId = context.ResolveByPath(matVfsPath.View());
                if (matId.IsValid())
                    context.DeclareBuildDep(matId);
            }
        }

        // Build NMSH blob
        NmshHeader header{};
        header.m_vertexCount = static_cast<uint32_t>(vertices.Size());
        header.m_indexCount = static_cast<uint32_t>(indices.Size());
        header.m_submeshCount = static_cast<uint32_t>(submeshes.Size());
        std::memcpy(header.m_aabbMin, aabbMin, sizeof(aabbMin));
        std::memcpy(header.m_aabbMax, aabbMax, sizeof(aabbMax));

        size_t total = NmshTotalSize(header);
        result.m_intermediateData.Resize(total);
        uint8_t* blob = result.m_intermediateData.Data();

        std::memcpy(blob, &header, sizeof(NmshHeader));
        std::memcpy(blob + sizeof(NmshHeader), submeshes.Data(), sizeof(SubMesh) * submeshes.Size());
        std::memcpy(blob + NmshVertexDataOffset(header), vertices.Data(), sizeof(MeshVertex) * vertices.Size());
        std::memcpy(blob + NmshIndexDataOffset(header), indices.Data(), sizeof(uint32_t) * indices.Size());

        result.m_success = true;
        return result;
    }
} // namespace nectar
