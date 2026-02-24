#include <larvae/larvae.h>
#include <nectar/mesh/obj_importer.h>
#include <nectar/mesh/mesh_data.h>
#include <nectar/pipeline/import_context.h>
#include <nectar/database/asset_database.h>
#include <nectar/hive/hive_document.h>
#include <nectar/core/asset_id.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <cmath>

namespace {

    auto& GetMeshAlloc()
    {
        static comb::ModuleAllocator alloc{"TestMesh", 8 * 1024 * 1024};
        return alloc.Get();
    }

    nectar::AssetId MakeId(uint64_t v)
    {
        uint8_t bytes[16] = {};
        std::memcpy(bytes, &v, sizeof(v));
        return nectar::AssetId::FromBytes(bytes);
    }

    // Minimal cube OBJ (8 vertices, 6 faces as quads, with normals and UVs)
    constexpr const char* kCubeObj =
        "# Unit cube\n"
        "v -0.5 -0.5  0.5\n"
        "v  0.5 -0.5  0.5\n"
        "v  0.5  0.5  0.5\n"
        "v -0.5  0.5  0.5\n"
        "v -0.5 -0.5 -0.5\n"
        "v  0.5 -0.5 -0.5\n"
        "v  0.5  0.5 -0.5\n"
        "v -0.5  0.5 -0.5\n"
        "vn  0  0  1\n"
        "vn  0  0 -1\n"
        "vn  0  1  0\n"
        "vn  0 -1  0\n"
        "vn  1  0  0\n"
        "vn -1  0  0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 1 1\n"
        "vt 0 1\n"
        "f 1/1/1 2/2/1 3/3/1 4/4/1\n"   // front
        "f 6/1/2 5/2/2 8/3/2 7/4/2\n"   // back
        "f 4/1/3 3/2/3 7/3/3 8/4/3\n"   // top
        "f 5/1/4 6/2/4 2/3/4 1/4/4\n"   // bottom
        "f 2/1/5 6/2/5 7/3/5 3/4/5\n"   // right
        "f 5/1/6 1/2/6 4/3/6 8/4/6\n";  // left

    // Triangle OBJ (no normals, no UVs — tests generated normals)
    constexpr const char* kTriangleObj =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n";

    wax::ByteSpan ObjSpan(const char* obj)
    {
        return wax::ByteSpan{reinterpret_cast<const uint8_t*>(obj), std::strlen(obj)};
    }

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarMesh", "ParseCubeOBJ", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(100)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(ObjSpan(kCubeObj), settings, ctx);
        larvae::AssertTrue(result.success);
        larvae::AssertTrue(result.intermediate_data.Size() > sizeof(nectar::NmshHeader));

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->magic, nectar::kNmshMagic);
        larvae::AssertEqual(header->version, uint32_t{2});

        // 6 quads triangulated = 12 triangles = 36 indices
        larvae::AssertEqual(header->index_count, uint32_t{36});
        // 6 faces * 4 unique vertex combos = 24 vertices (each face has unique normal)
        larvae::AssertEqual(header->vertex_count, uint32_t{24});
        larvae::AssertEqual(header->submesh_count, uint32_t{1});
    });

    auto t2 = larvae::RegisterTest("NectarMesh", "NmshBlobLayout", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(101)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(ObjSpan(kCubeObj), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());

        // Verify total size matches
        size_t expected_size = nectar::NmshTotalSize(*header);
        larvae::AssertEqual(result.intermediate_data.Size(), expected_size);

        // Verify submesh table is accessible
        auto* submeshes = reinterpret_cast<const nectar::SubMesh*>(
            result.intermediate_data.Data() + sizeof(nectar::NmshHeader));
        larvae::AssertEqual(submeshes[0].index_offset, uint32_t{0});
        larvae::AssertEqual(submeshes[0].index_count, header->index_count);

        // Verify vertex data is accessible
        auto* verts = reinterpret_cast<const nectar::MeshVertex*>(
            result.intermediate_data.Data() + nectar::NmshVertexDataOffset(*header));
        // All positions should be in [-0.5, 0.5]
        for (uint32_t i = 0; i < header->vertex_count; ++i)
        {
            for (int a = 0; a < 3; ++a)
            {
                larvae::AssertTrue(verts[i].position[a] >= -0.5f - 1e-5f);
                larvae::AssertTrue(verts[i].position[a] <=  0.5f + 1e-5f);
            }
        }

        // Verify index data is accessible and in range
        auto* idx = reinterpret_cast<const uint32_t*>(
            result.intermediate_data.Data() + nectar::NmshIndexDataOffset(*header));
        for (uint32_t i = 0; i < header->index_count; ++i)
        {
            larvae::AssertTrue(idx[i] < header->vertex_count);
        }
    });

    auto t3 = larvae::RegisterTest("NectarMesh", "AABB", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(102)};
        nectar::HiveDocument settings{alloc};

        auto result = importer.Import(ObjSpan(kCubeObj), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        constexpr float kTol = 1e-5f;
        for (int a = 0; a < 3; ++a)
        {
            larvae::AssertTrue(std::fabs(header->aabb_min[a] - (-0.5f)) < kTol);
            larvae::AssertTrue(std::fabs(header->aabb_max[a] -   0.5f)  < kTol);
        }
    });

    auto t4 = larvae::RegisterTest("NectarMesh", "ScaleSetting", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(103)};
        nectar::HiveDocument settings{alloc};
        settings.SetValue("import", "scale", nectar::HiveValue::MakeFloat(2.0));

        auto result = importer.Import(ObjSpan(kCubeObj), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        constexpr float kTol = 1e-5f;
        // Scaled by 2 → AABB should be [-1, 1]
        for (int a = 0; a < 3; ++a)
        {
            larvae::AssertTrue(std::fabs(header->aabb_min[a] - (-1.0f)) < kTol);
            larvae::AssertTrue(std::fabs(header->aabb_max[a] -   1.0f)  < kTol);
        }
    });

    auto t5 = larvae::RegisterTest("NectarMesh", "GeneratedNormals", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(104)};
        nectar::HiveDocument settings{alloc};

        // Triangle with no normals in OBJ — should generate face normal
        auto result = importer.Import(ObjSpan(kTriangleObj), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->vertex_count, uint32_t{3});
        larvae::AssertEqual(header->index_count, uint32_t{3});

        auto* verts = reinterpret_cast<const nectar::MeshVertex*>(
            result.intermediate_data.Data() + nectar::NmshVertexDataOffset(*header));

        // Triangle in XY plane → normal should be (0, 0, 1) or (0, 0, -1)
        constexpr float kTol = 1e-5f;
        for (uint32_t i = 0; i < 3; ++i)
        {
            larvae::AssertTrue(std::fabs(verts[i].normal[0]) < kTol);
            larvae::AssertTrue(std::fabs(verts[i].normal[1]) < kTol);
            larvae::AssertTrue(std::fabs(std::fabs(verts[i].normal[2]) - 1.0f) < kTol);
        }
    });

    auto t6 = larvae::RegisterTest("NectarMesh", "FlipUV", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(105)};
        nectar::HiveDocument settings{alloc};
        settings.SetValue("import", "flip_uv", nectar::HiveValue::MakeBool(true));

        auto result = importer.Import(ObjSpan(kCubeObj), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        auto* verts = reinterpret_cast<const nectar::MeshVertex*>(
            result.intermediate_data.Data() + nectar::NmshVertexDataOffset(*header));

        // Original UVs: (0,0), (1,0), (1,1), (0,1)
        // Flipped V:     (0,1), (1,1), (1,0), (0,0)
        // Check that at least one vertex has V > 0.5 (was 0) and one has V < 0.5 (was 1)
        bool found_high = false, found_low = false;
        for (uint32_t i = 0; i < header->vertex_count; ++i)
        {
            if (verts[i].uv[1] > 0.9f) found_high = true;
            if (verts[i].uv[1] < 0.1f) found_low = true;
        }
        larvae::AssertTrue(found_high);
        larvae::AssertTrue(found_low);
    });

    auto t7 = larvae::RegisterTest("NectarMesh", "InvalidData", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(106)};
        nectar::HiveDocument settings{alloc};

        const uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF};
        auto result = importer.Import(wax::ByteSpan{garbage, sizeof(garbage)}, settings, ctx);
        larvae::AssertFalse(result.success);
    });

    auto t8 = larvae::RegisterTest("NectarMesh", "EmptyObj", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(107)};
        nectar::HiveDocument settings{alloc};

        // Valid OBJ syntax but no geometry
        const char* empty_obj = "# empty\n";
        auto result = importer.Import(ObjSpan(empty_obj), settings, ctx);
        larvae::AssertFalse(result.success);
    });

    auto t9 = larvae::RegisterTest("NectarMesh", "Extensions", []() {
        nectar::ObjImporter importer;
        auto exts = importer.SourceExtensions();
        larvae::AssertEqual(exts.Size(), size_t{1});
        larvae::AssertTrue(wax::StringView{exts[0]}.Equals(".obj"));
    });

    auto t10 = larvae::RegisterTest("NectarMesh", "VersionAndTypeName", []() {
        nectar::ObjImporter importer;
        larvae::AssertEqual(importer.Version(), uint32_t{2});
        larvae::AssertTrue(importer.TypeName().Equals("Mesh"));
    });

    auto t11 = larvae::RegisterTest("NectarMesh", "QuadTriangulation", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(108)};
        nectar::HiveDocument settings{alloc};

        // Single quad → 2 triangles → 6 indices
        const char* quad_obj =
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 1 1 0\n"
            "v 0 1 0\n"
            "f 1 2 3 4\n";

        auto result = importer.Import(ObjSpan(quad_obj), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->index_count, uint32_t{6});
        larvae::AssertEqual(header->vertex_count, uint32_t{4});
    });

    auto t12 = larvae::RegisterTest("NectarMesh", "MeshVertexSize", []() {
        // Verify MeshVertex is exactly 36 bytes (pos + normal + uv + color)
        larvae::AssertEqual(sizeof(nectar::MeshVertex), size_t{36});
    });

    auto t13 = larvae::RegisterTest("NectarMesh", "SubMeshSize", []() {
        larvae::AssertEqual(sizeof(nectar::SubMesh), size_t{36});
    });

    auto t14 = larvae::RegisterTest("NectarMesh", "MaterialIndexDefault", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(110)};
        nectar::HiveDocument settings{alloc};

        // No materials → single submesh with material_index = -1
        auto result = importer.Import(ObjSpan(kCubeObj), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        larvae::AssertEqual(header->submesh_count, uint32_t{1});

        auto* submeshes = reinterpret_cast<const nectar::SubMesh*>(
            result.intermediate_data.Data() + sizeof(nectar::NmshHeader));
        larvae::AssertEqual(submeshes[0].material_index, int32_t{-1});
    });

    auto t15 = larvae::RegisterTest("NectarMesh", "SubMeshAABB", []() {
        auto& alloc = GetMeshAlloc();
        nectar::ObjImporter importer;
        nectar::AssetDatabase db{alloc};
        nectar::ImportContext ctx{alloc, db, MakeId(115)};
        nectar::HiveDocument settings{alloc};

        // Cube: vertices in [-0.5, 0.5], single submesh
        auto result = importer.Import(ObjSpan(kCubeObj), settings, ctx);
        larvae::AssertTrue(result.success);

        auto* header = reinterpret_cast<const nectar::NmshHeader*>(result.intermediate_data.Data());
        auto* submeshes = reinterpret_cast<const nectar::SubMesh*>(
            result.intermediate_data.Data() + sizeof(nectar::NmshHeader));

        // Single submesh AABB should match global AABB
        for (int a = 0; a < 3; ++a)
        {
            larvae::AssertTrue(submeshes[0].aabb_min[a] <= header->aabb_min[a] + 1e-5f);
            larvae::AssertTrue(submeshes[0].aabb_max[a] >= header->aabb_max[a] - 1e-5f);
        }
        // Cube is [-0.5, 0.5]
        larvae::AssertTrue(submeshes[0].aabb_min[0] < 0.f);
        larvae::AssertTrue(submeshes[0].aabb_max[0] > 0.f);
    });

}
