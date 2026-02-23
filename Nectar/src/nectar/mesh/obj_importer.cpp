#define TINYOBJLOADER_IMPLEMENTATION

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wshadow"
#include <tiny_obj_loader.h>
#pragma clang diagnostic pop

#include <nectar/mesh/obj_importer.h>
#include <nectar/hive/hive_document.h>
#include <cstring>
#include <cmath>
#include <string>
#include <map>
#include <unordered_map>
#include <sstream>
#include <vector>

namespace nectar
{
    wax::Span<const char* const> ObjImporter::SourceExtensions() const
    {
        static const char* const exts[] = {".obj"};
        return {exts, 1};
    }

    uint32_t ObjImporter::Version() const { return 2; }

    wax::StringView ObjImporter::TypeName() const { return "Mesh"; }

    // Hash for vertex deduplication
    struct VertexKey
    {
        int pos_idx;
        int norm_idx;
        int uv_idx;
        int mat_idx;

        bool operator==(const VertexKey& o) const
        {
            return pos_idx == o.pos_idx && norm_idx == o.norm_idx
                && uv_idx == o.uv_idx && mat_idx == o.mat_idx;
        }
    };

    struct VertexKeyHash
    {
        size_t operator()(const VertexKey& k) const
        {
            auto to_sz = [](int v) { return static_cast<size_t>(static_cast<unsigned>(v)); };
            size_t h = to_sz(k.pos_idx);
            h ^= to_sz(k.norm_idx) * size_t{2654435761u};
            h ^= to_sz(k.uv_idx) * size_t{40503u};
            h ^= to_sz(k.mat_idx) * size_t{2246822519u};
            return h;
        }
    };

    static uint32_t PackRGBA8(float r, float g, float b, float a = 1.f)
    {
        auto to_u8 = [](float v) -> uint32_t {
            return static_cast<uint32_t>(v * 255.f + 0.5f) & 0xFFu;
        };
        return to_u8(r) | (to_u8(g) << 8) | (to_u8(b) << 16) | (to_u8(a) << 24);
    }

    ImportResult ObjImporter::Import(wax::ByteSpan source_data,
                                      const HiveDocument& settings,
                                      ImportContext& /*context*/)
    {
        ImportResult result{};

        // Read settings
        auto scale = static_cast<float>(settings.GetFloat("import", "scale", 1.0));
        bool flip_uv = settings.GetBool("import", "flip_uv", false);
        bool gen_normals = settings.GetBool("import", "generate_normals", true);
        auto mtl_path = settings.GetString("import", "mtl_path", "");

        // Parse OBJ from memory
        std::string obj_text{reinterpret_cast<const char*>(source_data.Data()), source_data.Size()};
        std::istringstream stream{obj_text};

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        bool ok;
        if (!mtl_path.IsEmpty())
        {
            tinyobj::MaterialFileReader mtl_reader{std::string{mtl_path.Data(), mtl_path.Size()}};
            ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream, &mtl_reader);
        }
        else
        {
            ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream);
        }
        if (!ok || shapes.empty())
        {
            result.error_message = wax::String<>{"OBJ parse failed"};
            if (!err.empty())
            {
                result.error_message = wax::String<>{err.c_str()};
            }
            return result;
        }

        bool has_normals = !attrib.normals.empty();
        bool has_uvs = !attrib.texcoords.empty();

        // Deduplicate vertices across all shapes, group indices by material
        std::vector<MeshVertex> vertices;
        std::map<int, std::vector<uint32_t>> mat_indices; // material_id → indices
        std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertex_map;

        vertices.reserve(attrib.vertices.size() / 3);

        // AABB
        float aabb_min[3] = { 1e30f,  1e30f,  1e30f};
        float aabb_max[3] = {-1e30f, -1e30f, -1e30f};

        for (const auto& shape : shapes)
        {
            size_t idx_offset = 0;
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
            {
                auto fv = static_cast<size_t>(shape.mesh.num_face_vertices[f]);

                int mat_id = (!shape.mesh.material_ids.empty())
                    ? shape.mesh.material_ids[f] : -1;

                // Triangulate: fan from first vertex (works for convex faces)
                for (size_t v = 1; v + 1 < fv; ++v)
                {
                    size_t tri_verts[3] = {0, v, v + 1};

                    // Compute face normal if needed
                    float face_normal[3] = {0.f, 0.f, 0.f};
                    if (!has_normals && gen_normals)
                    {
                        auto get_pos = [&](size_t local_idx) -> const float* {
                            auto vi = static_cast<size_t>(shape.mesh.indices[idx_offset + local_idx].vertex_index);
                            return &attrib.vertices[3 * vi];
                        };
                        const float* p0 = get_pos(tri_verts[0]);
                        const float* p1 = get_pos(tri_verts[1]);
                        const float* p2 = get_pos(tri_verts[2]);

                        float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
                        float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

                        face_normal[0] = e1[1] * e2[2] - e1[2] * e2[1];
                        face_normal[1] = e1[2] * e2[0] - e1[0] * e2[2];
                        face_normal[2] = e1[0] * e2[1] - e1[1] * e2[0];

                        float len = std::sqrt(face_normal[0] * face_normal[0] +
                                              face_normal[1] * face_normal[1] +
                                              face_normal[2] * face_normal[2]);
                        if (len > 1e-8f)
                        {
                            face_normal[0] /= len;
                            face_normal[1] /= len;
                            face_normal[2] /= len;
                        }
                    }

                    for (size_t t = 0; t < 3; ++t)
                    {
                        const auto& idx = shape.mesh.indices[idx_offset + tri_verts[t]];

                        VertexKey key{idx.vertex_index, idx.normal_index, idx.texcoord_index, mat_id};
                        auto it = vertex_map.find(key);
                        if (it != vertex_map.end())
                        {
                            mat_indices[mat_id].push_back(it->second);
                            continue;
                        }

                        MeshVertex vert{};

                        auto vi = static_cast<size_t>(idx.vertex_index);
                        vert.position[0] = attrib.vertices[3 * vi + 0] * scale;
                        vert.position[1] = attrib.vertices[3 * vi + 1] * scale;
                        vert.position[2] = attrib.vertices[3 * vi + 2] * scale;

                        // AABB
                        for (int a = 0; a < 3; ++a)
                        {
                            if (vert.position[a] < aabb_min[a]) aabb_min[a] = vert.position[a];
                            if (vert.position[a] > aabb_max[a]) aabb_max[a] = vert.position[a];
                        }

                        // Normal
                        if (has_normals && idx.normal_index >= 0)
                        {
                            auto ni = static_cast<size_t>(idx.normal_index);
                            vert.normal[0] = attrib.normals[3 * ni + 0];
                            vert.normal[1] = attrib.normals[3 * ni + 1];
                            vert.normal[2] = attrib.normals[3 * ni + 2];
                        }
                        else if (gen_normals)
                        {
                            vert.normal[0] = face_normal[0];
                            vert.normal[1] = face_normal[1];
                            vert.normal[2] = face_normal[2];
                        }

                        // UV
                        if (has_uvs && idx.texcoord_index >= 0)
                        {
                            auto ti = static_cast<size_t>(idx.texcoord_index);
                            vert.uv[0] = attrib.texcoords[2 * ti + 0];
                            vert.uv[1] = flip_uv
                                ? 1.0f - attrib.texcoords[2 * ti + 1]
                                : attrib.texcoords[2 * ti + 1];
                        }

                        // Color from material Kd
                        if (mat_id >= 0 && mat_id < static_cast<int>(materials.size()))
                        {
                            const auto& mat = materials[static_cast<size_t>(mat_id)];
                            vert.color = PackRGBA8(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
                        }
                        else
                        {
                            vert.color = PackRGBA8(1.f, 1.f, 1.f);
                        }

                        uint32_t new_idx = static_cast<uint32_t>(vertices.size());
                        vertex_map[key] = new_idx;
                        vertices.push_back(vert);
                        mat_indices[mat_id].push_back(new_idx);
                    }
                }
                idx_offset += fv;
            }
        }

        // Flatten per-material buckets into final index buffer + submeshes
        std::vector<uint32_t> indices;
        std::vector<SubMesh> submeshes;
        indices.reserve(vertices.size());

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

        if (vertices.empty())
        {
            result.error_message = wax::String<>{"OBJ contains no geometry"};
            return result;
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

        // Header
        std::memcpy(blob, &header, sizeof(NmshHeader));

        // Submesh table
        std::memcpy(blob + sizeof(NmshHeader), submeshes.data(),
                     sizeof(SubMesh) * submeshes.size());

        // Vertex data
        std::memcpy(blob + NmshVertexDataOffset(header), vertices.data(),
                     sizeof(MeshVertex) * vertices.size());

        // Index data
        std::memcpy(blob + NmshIndexDataOffset(header), indices.data(),
                     sizeof(uint32_t) * indices.size());

        result.success = true;
        return result;
    }
}
