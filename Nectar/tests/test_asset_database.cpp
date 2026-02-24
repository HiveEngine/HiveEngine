#include <larvae/larvae.h>
#include <nectar/database/asset_database.h>
#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <comb/default_allocator.h>
#include <cstring>

namespace {

    auto& GetDbAlloc()
    {
        static comb::ModuleAllocator alloc{"TestAssetDB", 4 * 1024 * 1024};
        return alloc.Get();
    }

    nectar::AssetId MakeId(uint64_t v)
    {
        uint8_t bytes[16] = {};
        std::memcpy(bytes, &v, sizeof(v));
        return nectar::AssetId::FromBytes(bytes);
    }

    nectar::AssetRecord MakeRecord(uint64_t id, const char* path, const char* type, const char* name)
    {
        auto& alloc = GetDbAlloc();
        nectar::AssetRecord r{};
        r.uuid = MakeId(id);
        r.path = wax::String<>{alloc, path};
        r.type = wax::String<>{alloc, type};
        r.name = wax::String<>{alloc, name};
        r.content_hash = nectar::ContentHash::FromData(path, std::strlen(path));
        return r;
    }

    // =========================================================================
    // Insert / Find
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarAssetDB", "InsertAndFindByUuid", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        auto record = MakeRecord(1, "textures/hero.png", "Texture", "hero");
        larvae::AssertTrue(db.Insert(static_cast<nectar::AssetRecord&&>(record)));
        larvae::AssertEqual(db.Count(), size_t{1});

        auto* found = db.FindByUuid(MakeId(1));
        larvae::AssertNotNull(found);
        larvae::AssertTrue(found->path.View().Equals("textures/hero.png"));
    });

    auto t2 = larvae::RegisterTest("NectarAssetDB", "FindByPath", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "textures/hero.png", "Texture", "hero"));

        auto* found = db.FindByPath("textures/hero.png");
        larvae::AssertNotNull(found);
        larvae::AssertTrue(found->uuid == MakeId(1));
    });

    auto t3 = larvae::RegisterTest("NectarAssetDB", "FindByUuidNotFound", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        auto* found = db.FindByUuid(MakeId(99));
        larvae::AssertNull(found);
    });

    auto t4 = larvae::RegisterTest("NectarAssetDB", "FindByPathNotFound", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        auto* found = db.FindByPath("nonexistent");
        larvae::AssertNull(found);
    });

    // =========================================================================
    // Duplicates rejected
    // =========================================================================

    auto t5 = larvae::RegisterTest("NectarAssetDB", "DuplicateUuidRejected", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "a.png", "Texture", "a"));
        larvae::AssertFalse(db.Insert(MakeRecord(1, "b.png", "Texture", "b")));
        larvae::AssertEqual(db.Count(), size_t{1});
    });

    auto t6 = larvae::RegisterTest("NectarAssetDB", "DuplicatePathRejected", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "same.png", "Texture", "a"));
        larvae::AssertFalse(db.Insert(MakeRecord(2, "same.png", "Texture", "b")));
        larvae::AssertEqual(db.Count(), size_t{1});
    });

    // =========================================================================
    // Remove
    // =========================================================================

    auto t7 = larvae::RegisterTest("NectarAssetDB", "Remove", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "tex.png", "Texture", "tex"));
        larvae::AssertTrue(db.Remove(MakeId(1)));
        larvae::AssertEqual(db.Count(), size_t{0});
        larvae::AssertNull(db.FindByUuid(MakeId(1)));
        larvae::AssertNull(db.FindByPath("tex.png"));
    });

    auto t8 = larvae::RegisterTest("NectarAssetDB", "RemoveNonExistent", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        larvae::AssertFalse(db.Remove(MakeId(99)));
    });

    // =========================================================================
    // Update
    // =========================================================================

    auto t9 = larvae::RegisterTest("NectarAssetDB", "Update", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "old.png", "Texture", "old"));

        auto updated = MakeRecord(1, "new.png", "Texture", "new");
        larvae::AssertTrue(db.Update(MakeId(1), static_cast<nectar::AssetRecord&&>(updated)));

        larvae::AssertNull(db.FindByPath("old.png"));
        auto* found = db.FindByPath("new.png");
        larvae::AssertNotNull(found);
        larvae::AssertTrue(found->name.View().Equals("new"));
    });

    auto t10 = larvae::RegisterTest("NectarAssetDB", "UpdateNonExistent", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        larvae::AssertFalse(db.Update(MakeId(99), MakeRecord(99, "x", "T", "x")));
    });

    // =========================================================================
    // Queries
    // =========================================================================

    auto t11 = larvae::RegisterTest("NectarAssetDB", "FindByType", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "a.png", "Texture", "a"));
        db.Insert(MakeRecord(2, "b.glb", "Mesh", "b"));
        db.Insert(MakeRecord(3, "c.png", "Texture", "c"));

        wax::Vector<nectar::AssetRecord*> results{alloc};
        db.FindByType("Texture", results);
        larvae::AssertEqual(results.Size(), size_t{2});
    });

    auto t12 = larvae::RegisterTest("NectarAssetDB", "FindByLabel", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};

        auto r1 = MakeRecord(1, "a.png", "Texture", "a");
        r1.labels.PushBack(wax::String<>{alloc, "hero"});
        r1.labels.PushBack(wax::String<>{alloc, "character"});
        db.Insert(static_cast<nectar::AssetRecord&&>(r1));

        auto r2 = MakeRecord(2, "b.png", "Texture", "b");
        r2.labels.PushBack(wax::String<>{alloc, "environment"});
        db.Insert(static_cast<nectar::AssetRecord&&>(r2));

        wax::Vector<nectar::AssetRecord*> results{alloc};
        db.FindByLabel("hero", results);
        larvae::AssertEqual(results.Size(), size_t{1});
    });

    // =========================================================================
    // Contains
    // =========================================================================

    auto t13 = larvae::RegisterTest("NectarAssetDB", "Contains", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "a.png", "Texture", "a"));

        larvae::AssertTrue(db.Contains(MakeId(1)));
        larvae::AssertFalse(db.Contains(MakeId(2)));
        larvae::AssertTrue(db.ContainsPath("a.png"));
        larvae::AssertFalse(db.ContainsPath("b.png"));
    });

    // =========================================================================
    // DependencyGraph integration
    // =========================================================================

    auto t14 = larvae::RegisterTest("NectarAssetDB", "DepGraphIntegration", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "mat.mat", "Material", "mat"));
        db.Insert(MakeRecord(2, "tex.png", "Texture", "tex"));

        auto& graph = db.GetDependencyGraph();
        larvae::AssertTrue(graph.AddEdge(MakeId(1), MakeId(2), nectar::DepKind::Hard));

        wax::Vector<nectar::AssetId> deps{alloc};
        graph.GetDependencies(MakeId(1), nectar::DepKind::All, deps);
        larvae::AssertEqual(deps.Size(), size_t{1});
        larvae::AssertTrue(deps[0] == MakeId(2));
    });

    auto t15 = larvae::RegisterTest("NectarAssetDB", "RemoveCleansDeps", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};
        db.Insert(MakeRecord(1, "a.mat", "Material", "a"));
        db.Insert(MakeRecord(2, "b.png", "Texture", "b"));

        db.GetDependencyGraph().AddEdge(MakeId(1), MakeId(2), nectar::DepKind::Hard);
        db.Remove(MakeId(1));

        larvae::AssertFalse(db.GetDependencyGraph().HasEdge(MakeId(1), MakeId(2)));
    });

    // =========================================================================
    // Empty database
    // =========================================================================

    auto t16 = larvae::RegisterTest("NectarAssetDB", "EmptyDatabaseQueries", []() {
        auto& alloc = GetDbAlloc();
        nectar::AssetDatabase db{alloc};

        larvae::AssertEqual(db.Count(), size_t{0});
        larvae::AssertNull(db.FindByUuid(MakeId(1)));
        larvae::AssertNull(db.FindByPath("x"));
        larvae::AssertFalse(db.Contains(MakeId(1)));
        larvae::AssertFalse(db.ContainsPath("x"));

        wax::Vector<nectar::AssetRecord*> results{alloc};
        db.FindByType("Texture", results);
        larvae::AssertEqual(results.Size(), size_t{0});
    });

}
