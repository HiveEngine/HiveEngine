#define TINYOBJLOADER_IMPLEMENTATION

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wshadow"
#include <tiny_obj_loader.h>
#pragma clang diagnostic pop

#include <nectar/hive/hive_document.h>
#include <nectar/mesh/obj_importer.h>

#include <cmath>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nectar
{
    wax::Span<const char* const> ObjImporter::SourceExtensions() const {
        static const char* const exts[] = {".obj"};
        return {exts, 1};
    }

    uint32_t ObjImporter::Version() const {
        return 2;
    }

    wax::StringView ObjImporter::TypeName() const {
        return "Mesh";
    }

    struct VertexKey
    {
        int m_posIdx;
        int m_normIdx;
        int m_uvIdx;
        int m_matIdx;

        bool operator==(const VertexKey& other) const {
            return m_posIdx == other.m_posIdx && m_normIdx == other.m_normIdx && m_uvIdx == other.m_uvIdx &&
                   m_matIdx == other.m_matIdx;
        }
    };

    struct VertexKeyHash
    {
        size_t operator()(const VertexKey& key) const {
            auto toSize = [](int value) {
                return static_cast<size_t>(static_cast<unsigned>(value));
            };

            size_t hash = toSize(key.m_posIdx);
            hash ^= toSize(key.m_normIdx) * size_t{2654435761u};
            hash ^= toSize(key.m_uvIdx) * size_t{40503u};
            hash ^= toSize(key.m_matIdx) * size_t{2246822519u};
            return hash;
        }
    };

    static uint32_t PackRGBA8(float r, float g, float b, float a = 1.f) {
        auto toU8 = [](float value) -> uint32_t {
            return static_cast<uint32_t>(value * 255.f + 0.5f) & 0xFFu;
        };
        return toU8(r) | (toU8(g) << 8) | (toU8(b) << 16) | (toU8(a) << 24);
    }

    ImportResult ObjImporter::Import(wax::ByteSpan sourceData, const HiveDocument& settings,
                                     ImportContext& /*context*/) {
        ImportResult result{};

        auto scale = static_cast<float>(settings.GetFloat("import", "scale", 1.0));
        bool flipUv = settings.GetBool("import", "flip_uv", false);
        bool genNormals = settings.GetBool("import", "generate_normals", true);
        auto mtlPath = settings.GetString("import", "mtl_path", "");

        std::string objText{reinterpret_cast<const char*>(sourceData.Data()), sourceData.Size()};
        std::istringstream stream{objText};

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string err;

        bool ok = false;
        if (!mtlPath.IsEmpty())
        {
            tinyobj::MaterialFileReader mtlReader{std::string{mtlPath.Data(), mtlPath.Size()}};
            ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream, &mtlReader);
        }
        else
        {
            ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream);
        }

        if (!ok || shapes.empty())
        {
            result.m_errorMessage = wax::String{"OBJ parse failed"};
            if (!err.empty())
            {
                result.m_errorMessage = wax::String{err.c_str()};
            }
            return result;
        }

        bool hasNormals = !attrib.normals.empty();
        bool hasUvs = !attrib.texcoords.empty();

        std::vector<MeshVertex> vertices;
        std::map<int, std::vector<uint32_t>> matIndices;
        std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertexMap;

        vertices.reserve(attrib.vertices.size() / 3);

        float aabbMin[3] = {1e30f, 1e30f, 1e30f};
        float aabbMax[3] = {-1e30f, -1e30f, -1e30f};

        for (const auto& shape : shapes)
        {
            size_t idxOffset = 0;
            for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face)
            {
                auto fv = static_cast<size_t>(shape.mesh.num_face_vertices[face]);
                int matId = !shape.mesh.material_ids.empty() ? shape.mesh.material_ids[face] : -1;

                for (size_t vertex = 1; vertex + 1 < fv; ++vertex)
                {
                    size_t triVerts[3] = {0, vertex, vertex + 1};

                    float faceNormal[3] = {0.f, 0.f, 0.f};
                    if (!hasNormals && genNormals)
                    {
                        auto getPos = [&](size_t localIdx) -> const float* {
                            auto vi = static_cast<size_t>(shape.mesh.indices[idxOffset + localIdx].vertex_index);
                            return &attrib.vertices[3 * vi];
                        };

                        const float* p0 = getPos(triVerts[0]);
                        const float* p1 = getPos(triVerts[1]);
                        const float* p2 = getPos(triVerts[2]);

                        float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
                        float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

                        faceNormal[0] = e1[1] * e2[2] - e1[2] * e2[1];
                        faceNormal[1] = e1[2] * e2[0] - e1[0] * e2[2];
                        faceNormal[2] = e1[0] * e2[1] - e1[1] * e2[0];

                        float length = std::sqrt(faceNormal[0] * faceNormal[0] + faceNormal[1] * faceNormal[1] +
                                                 faceNormal[2] * faceNormal[2]);
                        if (length > 1e-8f)
                        {
                            faceNormal[0] /= length;
                            faceNormal[1] /= length;
                            faceNormal[2] /= length;
                        }
                    }

                    for (size_t triIndex = 0; triIndex < 3; ++triIndex)
                    {
                        const auto& idx = shape.mesh.indices[idxOffset + triVerts[triIndex]];

                        VertexKey key{idx.vertex_index, idx.normal_index, idx.texcoord_index, matId};
                        auto it = vertexMap.find(key);
                        if (it != vertexMap.end())
                        {
                            matIndices[matId].push_back(it->second);
                            continue;
                        }

                        MeshVertex vert{};
                        auto vi = static_cast<size_t>(idx.vertex_index);
                        vert.m_position[0] = attrib.vertices[3 * vi + 0] * scale;
                        vert.m_position[1] = attrib.vertices[3 * vi + 1] * scale;
                        vert.m_position[2] = attrib.vertices[3 * vi + 2] * scale;

                        for (int axis = 0; axis < 3; ++axis)
                        {
                            if (vert.m_position[axis] < aabbMin[axis])
                            {
                                aabbMin[axis] = vert.m_position[axis];
                            }
                            if (vert.m_position[axis] > aabbMax[axis])
                            {
                                aabbMax[axis] = vert.m_position[axis];
                            }
                        }

                        if (hasNormals && idx.normal_index >= 0)
                        {
                            auto ni = static_cast<size_t>(idx.normal_index);
                            vert.m_normal[0] = attrib.normals[3 * ni + 0];
                            vert.m_normal[1] = attrib.normals[3 * ni + 1];
                            vert.m_normal[2] = attrib.normals[3 * ni + 2];
                        }
                        else if (genNormals)
                        {
                            vert.m_normal[0] = faceNormal[0];
                            vert.m_normal[1] = faceNormal[1];
                            vert.m_normal[2] = faceNormal[2];
                        }

                        if (hasUvs && idx.texcoord_index >= 0)
                        {
                            auto ti = static_cast<size_t>(idx.texcoord_index);
                            vert.m_uv[0] = attrib.texcoords[2 * ti + 0];
                            vert.m_uv[1] = flipUv ? 1.0f - attrib.texcoords[2 * ti + 1] : attrib.texcoords[2 * ti + 1];
                        }

                        if (matId >= 0 && matId < static_cast<int>(materials.size()))
                        {
                            const auto& mat = materials[static_cast<size_t>(matId)];
                            vert.m_color = PackRGBA8(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
                        }
                        else
                        {
                            vert.m_color = PackRGBA8(1.f, 1.f, 1.f);
                        }

                        uint32_t newIdx = static_cast<uint32_t>(vertices.size());
                        vertexMap[key] = newIdx;
                        vertices.push_back(vert);
                        matIndices[matId].push_back(newIdx);
                    }
                }

                idxOffset += fv;
            }
        }

        std::vector<uint32_t> indices;
        std::vector<SubMesh> submeshes;
        indices.reserve(vertices.size());

        for (auto& [matId, idxBuf] : matIndices)
        {
            if (idxBuf.empty())
            {
                continue;
            }

            SubMesh sub{};
            sub.m_indexOffset = static_cast<uint32_t>(indices.size());
            sub.m_indexCount = static_cast<uint32_t>(idxBuf.size());
            sub.m_materialIndex = matId;
            indices.insert(indices.end(), idxBuf.begin(), idxBuf.end());
            submeshes.push_back(sub);
        }

        if (vertices.empty())
        {
            result.m_errorMessage = wax::String{"OBJ contains no geometry"};
            return result;
        }

        for (auto& sub : submeshes)
        {
            float smin[3] = {1e30f, 1e30f, 1e30f};
            float smax[3] = {-1e30f, -1e30f, -1e30f};
            for (uint32_t i = 0; i < sub.m_indexCount; ++i)
            {
                uint32_t vi = indices[sub.m_indexOffset + i];
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (vertices[vi].m_position[axis] < smin[axis])
                    {
                        smin[axis] = vertices[vi].m_position[axis];
                    }
                    if (vertices[vi].m_position[axis] > smax[axis])
                    {
                        smax[axis] = vertices[vi].m_position[axis];
                    }
                }
            }

            std::memcpy(sub.m_aabbMin, smin, sizeof(smin));
            std::memcpy(sub.m_aabbMax, smax, sizeof(smax));
        }

        NmshHeader header{};
        header.m_vertexCount = static_cast<uint32_t>(vertices.size());
        header.m_indexCount = static_cast<uint32_t>(indices.size());
        header.m_submeshCount = static_cast<uint32_t>(submeshes.size());
        std::memcpy(header.m_aabbMin, aabbMin, sizeof(aabbMin));
        std::memcpy(header.m_aabbMax, aabbMax, sizeof(aabbMax));

        size_t total = NmshTotalSize(header);
        result.m_intermediateData.Resize(total);
        uint8_t* blob = result.m_intermediateData.Data();

        std::memcpy(blob, &header, sizeof(NmshHeader));
        std::memcpy(blob + sizeof(NmshHeader), submeshes.data(), sizeof(SubMesh) * submeshes.size());
        std::memcpy(blob + NmshVertexDataOffset(header), vertices.data(), sizeof(MeshVertex) * vertices.size());
        std::memcpy(blob + NmshIndexDataOffset(header), indices.data(), sizeof(uint32_t) * indices.size());

        result.m_success = true;
        return result;
    }
} // namespace nectar
