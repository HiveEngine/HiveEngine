#include <larvae/larvae.h>
#include <nectar/mesh/gltf_importer.h>
#include <nectar/mesh/gltf_material.h>
#include <nectar/mesh/mesh_data.h>
#include <nectar/pipeline/import_context.h>
#include <nectar/database/asset_database.h>
#include <nectar/hive/hive_document.h>
#include <nectar/core/asset_id.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <cmath>

namespace {

    auto& GetGltfAlloc()
    {
        static comb::ModuleAllocator alloc{"TestGltf", 8 * 1024 * 1024};
        return alloc.Get();
    }

    nectar::AssetId MakeId(uint64_t v)
    {
        uint8_t bytes[16] = {};
        std::memcpy(bytes, &v, sizeof(v));
        return nectar::AssetId::FromBytes(bytes);
    }

    wax::ByteSpan GltfSpan(const char* json)
    {
        return wax::ByteSpan{reinterpret_cast<const uint8_t*>(json), std::strlen(json)};
    }

    // =========================================================================
    // Minimal glTF test data — base64 generated via Python struct.pack
    //
    // Triangle: 3 float3 positions + 3 uint16 indices + 2-byte pad = 44 bytes
    //   pos = {0,0,0, 1,0,0, 0,1,0}, idx = {0,1,2}
    // =========================================================================

    constexpr const char* kTriangleGltf =
        "{"
        "  \"asset\": { \"version\": \"2.0\" },"
        "  \"buffers\": [{ \"uri\": \"data:application/octet-stream;base64,"
            "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA=\","
            "\"byteLength\": 44 }],"
        "  \"bufferViews\": ["
        "    { \"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36, \"target\": 34962 },"
        "    { \"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6, \"target\": 34963 }"
        "  ],"
        "  \"accessors\": ["
        "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\","
        "      \"max\": [1,1,0], \"min\": [0,0,0] },"
        "    { \"bufferView\": 1, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\" }"
        "  ],"
        "  \"meshes\": [{"
        "    \"primitives\": [{"
        "      \"attributes\": { \"POSITION\": 0 },"
        "      \"indices\": 1"
        "    }]"
        "  }],"
        "  \"nodes\": [{ \"mesh\": 0 }],"
        "  \"scenes\": [{ \"nodes\": [0] }],"
        "  \"scene\": 0"
        "}";

    // Triangle with normals and UVs
    // Buffer: positions(36) + normals(36) + uvs(24) + indices(6+2pad) = 104 bytes

    constexpr const char* kTriangleWithAttribsGltf =
        "{"
        "  \"asset\": { \"version\": \"2.0\" },"
        "  \"buffers\": [{ \"uri\": \"data:application/octet-stream;base64,"
            "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/"
            "AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIAAAA=\","
            "\"byteLength\": 104 }],"
        "  \"bufferViews\": ["
        "    { \"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36, \"target\": 34962 },"
        "    { \"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 36, \"target\": 34962 },"
        "    { \"buffer\": 0, \"byteOffset\": 72, \"byteLength\": 24, \"target\": 34962 },"
        "    { \"buffer\": 0, \"byteOffset\": 96, \"byteLength\": 6,  \"target\": 34963 }"
        "  ],"
        "  \"accessors\": ["
        "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\","
        "      \"max\": [1,1,0], \"min\": [0,0,0] },"
        "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\" },"
        "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC2\" },"
        "    { \"bufferView\": 3, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\" }"
        "  ],"
        "  \"meshes\": [{"
        "    \"primitives\": [{"
        "      \"attributes\": { \"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2 },"
        "      \"indices\": 3"
        "    }]"
        "  }],"
        "  \"nodes\": [{ \"mesh\": 0 }],"
        "  \"scenes\": [{ \"nodes\": [0] }],"
        "  \"scene\": 0"
        "}";

    // Two primitives with different materials
    // Primitive 0: triangle (0,0,0)-(1,0,0)-(0,1,0) with material 0
    // Primitive 1: triangle (2,0,0)-(3,0,0)-(2,1,0) with material 1
    // Buffer: pos0(36) + idx0(6+2pad) + pos1(36) + idx1(6+2pad) = 88 bytes

    constexpr const char* kMultiMaterialGltf =
        "{"
        "  \"asset\": { \"version\": \"2.0\" },"
        "  \"buffers\": [{ \"uri\": \"data:application/octet-stream;base64,"
            "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAAAAABAAAAAAAAAAAAAAEBAAAAAAAAAAAAAAABAAACAPwAAAAAAAAEAAgAAAA==\","
            "\"byteLength\": 88 }],"
        "  \"bufferViews\": ["
        "    { \"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36, \"target\": 34962 },"
        "    { \"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6,  \"target\": 34963 },"
        "    { \"buffer\": 0, \"byteOffset\": 44, \"byteLength\": 36, \"target\": 34962 },"
        "    { \"buffer\": 0, \"byteOffset\": 80, \"byteLength\": 6,  \"target\": 34963 }"
        "  ],"
        "  \"accessors\": ["
        "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\","
        "      \"max\": [1,1,0], \"min\": [0,0,0] },"
        "    { \"bufferView\": 1, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\" },"
        "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\","
        "      \"max\": [3,1,0], \"min\": [2,0,0] },"
        "    { \"bufferView\": 3, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\" }"
        "  ],"
        "  \"materials\": ["
        "    { \"pbrMetallicRoughness\": { \"baseColorFactor\": [1, 0, 0, 1] } },"
        "    { \"pbrMetallicRoughness\": { \"baseColorFactor\": [0, 1, 0, 1] } }"
        "  ],"
        "  \"meshes\": [{"
        "    \"primitives\": ["
        "      { \"attributes\": { \"POSITION\": 0 }, \"indices\": 1, \"material\": 0 },"
        "      { \"attributes\": { \"POSITION\": 2 }, \"indices\": 3, \"material\": 1 }"
        "    ]"
        "  }],"
        "  \"nodes\": [{ \"mesh\": 0 }],"
        "  \"scenes\": [{ \"nodes\": [0] }],"
        "  \"scene\": 0"
        "}";

    // Material with texture reference
    constexpr const char* kMaterialWithTextureGltf =
        "{"
        "  \"asset\": { \"version\": \"2.0\" },"
        "  \"images\": [{ \"uri\": \"textures/albedo.png\" }, { \"uri\": \"textures/normal.png\" }],"
        "  \"textures\": [{ \"source\": 0 }, { \"source\": 1 }],"
        "  \"materials\": ["
        "    { \"pbrMetallicRoughness\": {"
        "        \"baseColorTexture\": { \"index\": 0 },"
        "        \"baseColorFactor\": [0.8, 0.2, 0.1, 1.0]"
        "    }},"
        "    { \"pbrMetallicRoughness\": {"
        "        \"baseColorFactor\": [1, 1, 1, 1]"
        "    }},"
        "    { \"pbrMetallicRoughness\": {"
        "        \"baseColorTexture\": { \"index\": 1 },"
        "        \"baseColorFactor\": [1, 1, 1, 1]"
        "    }}"
        "  ],"
        "  \"buffers\": [{ \"uri\": \"data:application/octet-stream;base64,"
            "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAEAAgAAAA==\","
            "\"byteLength\": 44 }],"
        "  \"bufferViews\": ["
        "    { \"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36, \"target\": 34962 },"
        "    { \"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6,  \"target\": 34963 }"
        "  ],"
        "  \"accessors\": ["
        "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\","
        "      \"max\": [1,1,0], \"min\": [0,0,0] },"
        "    { \"bufferView\": 1, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\" }"
        "  ],"
        "  \"meshes\": [{"
        "    \"primitives\": [{"
        "      \"attributes\": { \"POSITION\": 0 },"
        "      \"indices\": 1,"
        "      \"material\": 0"
        "    }]"
        "  }],"
        "  \"nodes\": [{ \"mesh\": 0 }],"
        "  \"scenes\": [{ \"nodes\": [0] }],"
        "  \"scene\": 0"
        "}";

    // =========================================================================
    // Tests
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarGltf", "ParseTriangle", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(200)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(GltfSpan(kTriangleGltf), settings, ctx);
        larvae::AssertTrue(result.success);
        larvae::AssertTrue(result.intermediate_data.Size() > sizeof(nectar::NmshHeader));

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->magic, nectar::kNmshMagic);
        larvae::AssertEqual(header->version, uint32_t{2});
        larvae::AssertEqual(header->vertex_count, uint32_t{3});
        larvae::AssertEqual(header->index_count, uint32_t{3});
        larvae::AssertEqual(header->submesh_count, uint32_t{1});
    });

    auto t2 = larvae::RegisterTest("NectarGltf", "NmshBlobLayout", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(201)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(GltfSpan(kTriangleGltf), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(result.intermediate_data.Size(), nectar::NmshTotalSize(*header));

        // Submesh table
        auto* submeshes = reinterpret_cast<const nectar::SubMesh*>(
            result.intermediate_data.Data() + sizeof(nectar::NmshHeader));
        larvae::AssertEqual(submeshes[0].index_offset, uint32_t{0});
        larvae::AssertEqual(submeshes[0].index_count, uint32_t{3});

        // Index data in range
        auto* idx = reinterpret_cast<const uint32_t*>(
            result.intermediate_data.Data() + nectar::NmshIndexDataOffset(*header));
        for (uint32_t i = 0; i < header->index_count; ++i)
            larvae::AssertTrue(idx[i] < header->vertex_count);
    });

    auto t3 = larvae::RegisterTest("NectarGltf", "AABB", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(202)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(GltfSpan(kTriangleGltf), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        constexpr float kTol = 1e-5f;
        // Triangle at (0,0,0), (1,0,0), (0,1,0)
        for (int a = 0; a < 3; ++a)
            larvae::AssertTrue(std::fabs(header->aabb_min[a]) < kTol);
        larvae::AssertTrue(std::fabs(header->aabb_max[0] - 1.0f) < kTol);
        larvae::AssertTrue(std::fabs(header->aabb_max[1] - 1.0f) < kTol);
        larvae::AssertTrue(std::fabs(header->aabb_max[2]) < kTol);
    });

    auto t4 = larvae::RegisterTest("NectarGltf", "ScaleSetting", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(203)};
        nectar::HiveDocument settings{alloc};
        settings.SetValue("import", "scale", nectar::HiveValue::MakeFloat(2.0));

        auto result = importer.Import(GltfSpan(kTriangleGltf), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        constexpr float kTol = 1e-5f;
        larvae::AssertTrue(std::fabs(header->aabb_max[0] - 2.0f) < kTol);
        larvae::AssertTrue(std::fabs(header->aabb_max[1] - 2.0f) < kTol);
    });

    auto t5 = larvae::RegisterTest("NectarGltf", "GeneratedNormals", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(204)};
        nectar::HiveDocument settings{alloc};

        // Triangle with no normals in glTF
        auto result = importer.Import(GltfSpan(kTriangleGltf), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        auto* verts = reinterpret_cast<const nectar::MeshVertex*>(
            result.intermediate_data.Data() + nectar::NmshVertexDataOffset(*header));

        // Triangle in XY plane -> normal = (0,0,1) or (0,0,-1)
        constexpr float kTol = 1e-5f;
        for (uint32_t i = 0; i < 3; ++i)
        {
            larvae::AssertTrue(std::fabs(verts[i].normal[0]) < kTol);
            larvae::AssertTrue(std::fabs(verts[i].normal[1]) < kTol);
            larvae::AssertTrue(std::fabs(std::fabs(verts[i].normal[2]) - 1.0f) < kTol);
        }
    });

    auto t6 = larvae::RegisterTest("NectarGltf", "WithNormalsAndUVs", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(205)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(GltfSpan(kTriangleWithAttribsGltf), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->vertex_count, uint32_t{3});

        auto* verts = reinterpret_cast<const nectar::MeshVertex*>(
            result.intermediate_data.Data() + nectar::NmshVertexDataOffset(*header));

        // All normals should be (0,0,1)
        constexpr float kTol = 1e-5f;
        for (uint32_t i = 0; i < 3; ++i)
        {
            larvae::AssertTrue(std::fabs(verts[i].normal[2] - 1.0f) < kTol);
        }

        // Check UVs exist (at least one vertex has non-zero UV)
        bool has_uv = false;
        for (uint32_t i = 0; i < 3; ++i)
        {
            if (verts[i].uv[0] > 0.01f || verts[i].uv[1] > 0.01f)
                has_uv = true;
        }
        larvae::AssertTrue(has_uv);
    });

    auto t7 = larvae::RegisterTest("NectarGltf", "MultiMaterial", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(206)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(GltfSpan(kMultiMaterialGltf), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->vertex_count, uint32_t{6}); // 2 triangles
        larvae::AssertEqual(header->index_count, uint32_t{6});
        larvae::AssertEqual(header->submesh_count, uint32_t{2});

        auto* submeshes = reinterpret_cast<const nectar::SubMesh*>(
            result.intermediate_data.Data() + sizeof(nectar::NmshHeader));

        // Each submesh: 3 indices, different material
        larvae::AssertEqual(submeshes[0].index_count, uint32_t{3});
        larvae::AssertEqual(submeshes[1].index_count, uint32_t{3});
        larvae::AssertTrue(submeshes[0].material_index != submeshes[1].material_index);
        larvae::AssertTrue(submeshes[0].material_index >= 0);
        larvae::AssertTrue(submeshes[1].material_index >= 0);
    });

    auto t8 = larvae::RegisterTest("NectarGltf", "Extensions", []() {
        nectar::GltfImporter importer;
        auto exts = importer.SourceExtensions();
        larvae::AssertEqual(exts.Size(), size_t{2});
        larvae::AssertTrue(wax::StringView{exts[0]}.Equals(".gltf"));
        larvae::AssertTrue(wax::StringView{exts[1]}.Equals(".glb"));
    });

    auto t9 = larvae::RegisterTest("NectarGltf", "VersionAndTypeName", []() {
        nectar::GltfImporter importer;
        larvae::AssertEqual(importer.Version(), uint32_t{2});
        larvae::AssertTrue(importer.TypeName().Equals("Mesh"));
    });

    auto t10 = larvae::RegisterTest("NectarGltf", "InvalidData", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(207)};
        nectar::HiveDocument settings{alloc};

        const uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF};
        auto result = importer.Import(wax::ByteSpan{garbage, sizeof(garbage)}, settings, ctx);
        larvae::AssertFalse(result.success);
    });

    auto t11 = larvae::RegisterTest("NectarGltf", "EmptyMeshes", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(208)};
        nectar::HiveDocument settings{alloc};

        // Valid glTF but no meshes
        const char* empty_gltf = "{ \"asset\": { \"version\": \"2.0\" } }";
        auto result = importer.Import(GltfSpan(empty_gltf), settings, ctx);
        larvae::AssertFalse(result.success);
    });

    auto t12 = larvae::RegisterTest("NectarGltf", "MaterialExtraction", []() {
        auto& alloc = GetGltfAlloc();

        auto materials = nectar::ParseGltfMaterials(GltfSpan(kMaterialWithTextureGltf), alloc);
        larvae::AssertEqual(materials.Size(), size_t{3});

        // Material 0: has base color texture
        larvae::AssertEqual(materials[0].material_index, int32_t{0});
        larvae::AssertTrue(materials[0].base_color_texture.Size() > 0);

        // Verify texture path
        auto path0 = materials[0].base_color_texture.View();
        larvae::AssertTrue(path0.Equals("textures/albedo.png"));

        // Material 1: no texture
        larvae::AssertEqual(materials[1].material_index, int32_t{1});
        larvae::AssertTrue(materials[1].base_color_texture.Size() == 0);

        // Material 2: has texture (normal map as base color for test)
        larvae::AssertTrue(materials[2].base_color_texture.View().Equals("textures/normal.png"));
    });

    auto t13 = larvae::RegisterTest("NectarGltf", "MaterialBaseColorFactor", []() {
        auto& alloc = GetGltfAlloc();

        auto materials = nectar::ParseGltfMaterials(GltfSpan(kMaterialWithTextureGltf), alloc);
        larvae::AssertEqual(materials.Size(), size_t{3});

        constexpr float kTol = 1e-5f;
        // Material 0: baseColorFactor = [0.8, 0.2, 0.1, 1.0]
        larvae::AssertTrue(std::fabs(materials[0].base_color_factor[0] - 0.8f) < kTol);
        larvae::AssertTrue(std::fabs(materials[0].base_color_factor[1] - 0.2f) < kTol);
        larvae::AssertTrue(std::fabs(materials[0].base_color_factor[2] - 0.1f) < kTol);
        larvae::AssertTrue(std::fabs(materials[0].base_color_factor[3] - 1.0f) < kTol);
    });

    auto t14 = larvae::RegisterTest("NectarGltf", "MaterialDefaultNoData", []() {
        auto& alloc = GetGltfAlloc();

        // glTF with no materials at all
        auto materials = nectar::ParseGltfMaterials(GltfSpan(kTriangleGltf), alloc);
        larvae::AssertEqual(materials.Size(), size_t{0});
    });

    auto t15 = larvae::RegisterTest("NectarGltf", "MaterialIndexDefault", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(209)};
        nectar::HiveDocument settings{alloc};

        // Triangle with no material -> material_index = -1
        auto result = importer.Import(GltfSpan(kTriangleGltf), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        auto* submeshes = reinterpret_cast<const nectar::SubMesh*>(
            result.intermediate_data.Data() + sizeof(nectar::NmshHeader));
        larvae::AssertEqual(submeshes[0].material_index, int32_t{-1});
    });

    auto t16 = larvae::RegisterTest("NectarGltf", "SubMeshAABB", []() {
        auto& alloc = GetGltfAlloc();
        nectar::GltfImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(210)};
        nectar::HiveDocument settings{alloc};

        // Triangle: vertices at (0,0,0), (1,0,0), (0,1,0)
        auto result = importer.Import(GltfSpan(kTriangleGltf), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        auto* submeshes = reinterpret_cast<const nectar::SubMesh*>(
            result.intermediate_data.Data() + sizeof(nectar::NmshHeader));

        // Single submesh — AABB should match global
        for (int a = 0; a < 3; ++a)
        {
            larvae::AssertTrue(submeshes[0].aabb_min[a] <= header->aabb_min[a] + 1e-5f);
            larvae::AssertTrue(submeshes[0].aabb_max[a] >= header->aabb_max[a] - 1e-5f);
        }
    });

}
