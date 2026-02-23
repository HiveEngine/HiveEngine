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
#include <nectar/hive/hive_document.h>
#include <cstring>
#include <cmath>
#include <string>
#include <map>
#include <vector>

namespace nectar
{
    wax::Span<const char* const> GltfImporter::SourceExtensions() const
    {
        static const char* const exts[] = {".gltf", ".glb"};
        return {exts, 2};
    }

    uint32_t GltfImporter::Version() const { return 2; }

    wax::StringView GltfImporter::TypeName() const { return "Mesh"; }

    static uint32_t PackRGBA8(float r, float g, float b, float a = 1.f)
    {
        auto to_u8 = [](float v) -> uint32_t {
            return static_cast<uint32_t>(v * 255.f + 0.5f) & 0xFFu;
        };
        return to_u8(r) | (to_u8(g) << 8) | (to_u8(b) << 16) | (to_u8(a) << 24);
    }

    ImportResult GltfImporter::Import(wax::ByteSpan source_data,
                                       const HiveDocument& settings,
                                       ImportContext& /*context*/)
    {
        ImportResult result{};

        auto scale = static_cast<float>(settings.GetFloat("import", "scale", 1.0));
        bool flip_uv = settings.GetBool("import", "flip_uv", false);
        bool gen_normals = settings.GetBool("import", "generate_normals", true);
        auto base_path = settings.GetString("import", "base_path", "");

        // Parse glTF/GLB from memory
        cgltf_options options{};
        cgltf_data* data = nullptr;
        cgltf_result res = cgltf_parse(&options, source_data.Data(), source_data.Size(), &data);
        if (res != cgltf_result_success)
        {
            result.error_message = wax::String<>{"glTF parse failed"};
            return result;
        }

        // Load buffers: external .bin for .gltf, embedded for .glb
        if (!base_path.IsEmpty())
        {
            std::string path{base_path.Data(), base_path.Size()};
            res = cgltf_load_buffers(&options, data, path.c_str());
        }
        else
        {
            res = cgltf_load_buffers(&options, data, nullptr);
        }
        if (res != cgltf_result_success)
        {
            cgltf_free(data);
            result.error_message = wax::String<>{"glTF buffer load failed"};
            return result;
        }

        // Collect geometry from all meshes/primitives
        std::vector<MeshVertex> vertices;
        std::map<int, std::vector<uint32_t>> mat_indices;

        float aabb_min[3] = { 1e30f,  1e30f,  1e30f};
        float aabb_max[3] = {-1e30f, -1e30f, -1e30f};

        for (cgltf_size mi = 0; mi < data->meshes_count; ++mi)
        {
            const auto& mesh = data->meshes[mi];

            for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi)
            {
                const auto& prim = mesh.primitives[pi];

                // Only triangles
                if (prim.type != cgltf_primitive_type_triangles) continue;

                // Find attribute accessors
                const cgltf_accessor* pos_acc = nullptr;
                const cgltf_accessor* norm_acc = nullptr;
                const cgltf_accessor* uv_acc = nullptr;
                const cgltf_accessor* color_acc = nullptr;

                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
                {
                    const auto& attr = prim.attributes[ai];
                    if (attr.type == cgltf_attribute_type_position) pos_acc = attr.data;
                    else if (attr.type == cgltf_attribute_type_normal) norm_acc = attr.data;
                    else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uv_acc = attr.data;
                    else if (attr.type == cgltf_attribute_type_color && attr.index == 0) color_acc = attr.data;
                }

                if (!pos_acc) continue;

                // Material index (offset in data->materials array)
                int mat_id = -1;
                if (prim.material)
                    mat_id = static_cast<int>(prim.material - data->materials);

                // Base color factor from PBR material
                float base_color[4] = {1.f, 1.f, 1.f, 1.f};
                if (prim.material && prim.material->has_pbr_metallic_roughness)
                    std::memcpy(base_color, prim.material->pbr_metallic_roughness.base_color_factor, 4 * sizeof(float));

                uint32_t base_vertex = static_cast<uint32_t>(vertices.size());
                auto vert_count = static_cast<cgltf_size>(pos_acc->count);

                // Extract vertices
                for (cgltf_size vi = 0; vi < vert_count; ++vi)
                {
                    MeshVertex vert{};

                    // Position
                    float pos[3] = {};
                    cgltf_accessor_read_float(pos_acc, vi, pos, 3);
                    vert.position[0] = pos[0] * scale;
                    vert.position[1] = pos[1] * scale;
                    vert.position[2] = pos[2] * scale;

                    for (int a = 0; a < 3; ++a)
                    {
                        if (vert.position[a] < aabb_min[a]) aabb_min[a] = vert.position[a];
                        if (vert.position[a] > aabb_max[a]) aabb_max[a] = vert.position[a];
                    }

                    // Normal
                    if (norm_acc)
                    {
                        float norm[3] = {};
                        cgltf_accessor_read_float(norm_acc, vi, norm, 3);
                        vert.normal[0] = norm[0];
                        vert.normal[1] = norm[1];
                        vert.normal[2] = norm[2];
                    }

                    // UV
                    if (uv_acc)
                    {
                        float uv[2] = {};
                        cgltf_accessor_read_float(uv_acc, vi, uv, 2);
                        vert.uv[0] = uv[0];
                        vert.uv[1] = flip_uv ? 1.0f - uv[1] : uv[1];
                    }

                    // Vertex color from attribute, or material base color
                    if (color_acc)
                    {
                        float col[4] = {1.f, 1.f, 1.f, 1.f};
                        cgltf_accessor_read_float(color_acc, vi, col, 4);
                        vert.color = PackRGBA8(col[0], col[1], col[2], col[3]);
                    }
                    else
                    {
                        vert.color = PackRGBA8(base_color[0], base_color[1], base_color[2], base_color[3]);
                    }

                    vertices.push_back(vert);
                }

                // Extract indices
                auto& idx_buf = mat_indices[mat_id];
                if (prim.indices)
                {
                    for (cgltf_size ii = 0; ii < prim.indices->count; ++ii)
                    {
                        auto idx = cgltf_accessor_read_index(prim.indices, ii);
                        idx_buf.push_back(base_vertex + static_cast<uint32_t>(idx));
                    }
                }
                else
                {
                    for (uint32_t vi = 0; vi < static_cast<uint32_t>(vert_count); ++vi)
                        idx_buf.push_back(base_vertex + vi);
                }

                // Generate face normals if source has none
                if (!norm_acc && gen_normals)
                {
                    size_t idx_end = idx_buf.size();
                    size_t idx_start = idx_end - (prim.indices ? prim.indices->count : vert_count);
                    size_t tri_count = (idx_end - idx_start) / 3;

                    for (size_t ti = 0; ti < tri_count; ++ti)
                    {
                        uint32_t i0 = idx_buf[idx_start + ti * 3 + 0];
                        uint32_t i1 = idx_buf[idx_start + ti * 3 + 1];
                        uint32_t i2 = idx_buf[idx_start + ti * 3 + 2];

                        float* p0 = vertices[i0].position;
                        float* p1 = vertices[i1].position;
                        float* p2 = vertices[i2].position;

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

                        vertices[i0].normal[0] = n[0]; vertices[i0].normal[1] = n[1]; vertices[i0].normal[2] = n[2];
                        vertices[i1].normal[0] = n[0]; vertices[i1].normal[1] = n[1]; vertices[i1].normal[2] = n[2];
                        vertices[i2].normal[0] = n[0]; vertices[i2].normal[1] = n[1]; vertices[i2].normal[2] = n[2];
                    }
                }
            }
        }

        cgltf_free(data);

        if (vertices.empty())
        {
            result.error_message = wax::String<>{"glTF contains no geometry"};
            return result;
        }

        // Flatten per-material buckets into final index buffer + submeshes
        std::vector<uint32_t> indices;
        std::vector<SubMesh> submeshes;

        for (auto& [mat_id, idx_buf] : mat_indices)
        {
            if (idx_buf.empty()) continue;
            SubMesh sub{};
            sub.index_offset = static_cast<uint32_t>(indices.size());
            sub.index_count = static_cast<uint32_t>(idx_buf.size());
            sub.material_index = mat_id;
            indices.insert(indices.end(), idx_buf.begin(), idx_buf.end());
            submeshes.push_back(sub);
        }

        // Per-submesh AABB
        for (auto& sub : submeshes)
        {
            float smin[3] = { 1e30f,  1e30f,  1e30f};
            float smax[3] = {-1e30f, -1e30f, -1e30f};
            for (uint32_t i = 0; i < sub.index_count; ++i)
            {
                uint32_t vi = indices[sub.index_offset + i];
                for (int a = 0; a < 3; ++a)
                {
                    if (vertices[vi].position[a] < smin[a]) smin[a] = vertices[vi].position[a];
                    if (vertices[vi].position[a] > smax[a]) smax[a] = vertices[vi].position[a];
                }
            }
            std::memcpy(sub.aabb_min, smin, sizeof(smin));
            std::memcpy(sub.aabb_max, smax, sizeof(smax));
        }

        // Build NMSH blob
        NmshHeader header{};
        header.vertex_count = static_cast<uint32_t>(vertices.size());
        header.index_count = static_cast<uint32_t>(indices.size());
        header.submesh_count = static_cast<uint32_t>(submeshes.size());
        std::memcpy(header.aabb_min, aabb_min, sizeof(aabb_min));
        std::memcpy(header.aabb_max, aabb_max, sizeof(aabb_max));

        size_t total = NmshTotalSize(header);
        result.intermediate_data.Resize(total);
        uint8_t* blob = result.intermediate_data.Data();

        std::memcpy(blob, &header, sizeof(NmshHeader));
        std::memcpy(blob + sizeof(NmshHeader), submeshes.data(),
                     sizeof(SubMesh) * submeshes.size());
        std::memcpy(blob + NmshVertexDataOffset(header), vertices.data(),
                     sizeof(MeshVertex) * vertices.size());
        std::memcpy(blob + NmshIndexDataOffset(header), indices.data(),
                     sizeof(uint32_t) * indices.size());

        result.success = true;
        return result;
    }
}
