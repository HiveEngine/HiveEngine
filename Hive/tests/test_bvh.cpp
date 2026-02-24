#include <larvae/larvae.h>
#include <hive/math/bvh.h>
#include <hive/math/transforms.h>
#include <comb/buddy_allocator.h>

namespace {

    using namespace hive::math;

    constexpr size_t kAllocSize = 1024 * 1024;

    AABB MakeBox(float cx, float cy, float cz, float h = 0.5f)
    {
        return {{cx - h, cy - h, cz - h}, {cx + h, cy + h, cz + h}};
    }

    Frustum MakeLookAtFrustum(Float3 eye, Float3 target, float fov, float aspect, float z_near, float z_far)
    {
        Mat4 view = LookAt(eye, target, {0.f, 1.f, 0.f});
        Mat4 proj = Perspective(fov, aspect, z_near, z_far);
        return ExtractFrustum(proj * view);
    }

    // =========================================================================
    // Build
    // =========================================================================

    auto t_build_empty = larvae::RegisterTest("HiveBVH", "BuildEmpty", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        bvh.Build(nullptr, 0);
        larvae::AssertTrue(bvh.IsEmpty());
        larvae::AssertEqual(bvh.ItemCount(), 0u);
    });

    auto t_build_single = larvae::RegisterTest("HiveBVH", "BuildSingle", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        AABB box = MakeBox(0.f, 0.f, 0.f);
        bvh.Build(&box, 1);

        larvae::AssertTrue(!bvh.IsEmpty());
        larvae::AssertEqual(bvh.ItemCount(), 1u);

        uint32_t found = 0;
        Frustum frustum = MakeLookAtFrustum({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}, Radians(90.f), 1.f, 0.1f, 100.f);
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 1u);
    });

    auto t_build_multiple = larvae::RegisterTest("HiveBVH", "BuildMultiple", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        constexpr uint32_t N = 10;
        AABB boxes[N];
        for (uint32_t i = 0; i < N; ++i)
            boxes[i] = MakeBox(static_cast<float>(i) * 3.f, 0.f, 0.f);

        bvh.Build(boxes, N);
        larvae::AssertEqual(bvh.ItemCount(), N);
        larvae::AssertTrue(bvh.NodeCount() <= N * 2 - 1);
    });

    // =========================================================================
    // Frustum culling
    // =========================================================================

    auto t_frustum_all = larvae::RegisterTest("HiveBVH", "FrustumCullAll", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        constexpr uint32_t N = 8;
        AABB boxes[N];
        uint32_t idx = 0;
        for (int x = 0; x < 2; ++x)
            for (int y = 0; y < 2; ++y)
                for (int z = 0; z < 2; ++z)
                    boxes[idx++] = MakeBox(static_cast<float>(x) * 2.f, static_cast<float>(y) * 2.f,
                                           static_cast<float>(z) * 2.f, 0.4f);
        bvh.Build(boxes, N);

        Frustum frustum = MakeLookAtFrustum({1.f, 1.f, 20.f}, {1.f, 1.f, 0.f}, Radians(90.f), 1.f, 0.1f, 100.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, N);
    });

    auto t_frustum_none = larvae::RegisterTest("HiveBVH", "FrustumCullNone", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        constexpr uint32_t N = 5;
        AABB boxes[N];
        for (uint32_t i = 0; i < N; ++i)
            boxes[i] = MakeBox(static_cast<float>(i), 0.f, 0.f, 0.3f);
        bvh.Build(boxes, N);

        Frustum frustum = MakeLookAtFrustum({0.f, 0.f, -50.f}, {0.f, 0.f, -100.f}, Radians(60.f), 1.f, 0.1f, 50.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 0u);
    });

    auto t_frustum_partial = larvae::RegisterTest("HiveBVH", "FrustumCullPartial", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        constexpr uint32_t N = 10;
        AABB boxes[N];
        for (uint32_t i = 0; i < N; ++i)
            boxes[i] = MakeBox(static_cast<float>(i) * 3.f, 0.f, 0.f, 0.4f);
        bvh.Build(boxes, N);

        Frustum frustum = MakeLookAtFrustum({2.5f, 0.f, 10.f}, {2.5f, 0.f, 0.f}, Radians(30.f), 1.f, 0.1f, 50.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertTrue(found > 0);
        larvae::AssertTrue(found < N);
    });

    // =========================================================================
    // Dynamic insert
    // =========================================================================

    auto t_insert_single = larvae::RegisterTest("HiveBVH", "InsertSingle", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        auto proxy = bvh.Insert(MakeBox(0.f, 0.f, 0.f));
        larvae::AssertTrue(proxy.IsValid());
        larvae::AssertTrue(!bvh.IsEmpty());
        larvae::AssertEqual(bvh.ItemCount(), 1u);

        Frustum frustum = MakeLookAtFrustum({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}, Radians(90.f), 1.f, 0.1f, 100.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 1u);
    });

    auto t_insert_multiple = larvae::RegisterTest("HiveBVH", "InsertMultiple", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        constexpr uint32_t N = 20;
        for (uint32_t i = 0; i < N; ++i)
            (void)bvh.Insert(MakeBox(static_cast<float>(i) * 2.f, 0.f, 0.f));

        larvae::AssertEqual(bvh.ItemCount(), N);

        Frustum frustum = MakeLookAtFrustum({19.f, 0.f, 50.f}, {19.f, 0.f, 0.f}, Radians(90.f), 1.f, 0.1f, 200.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, N);
    });

    // =========================================================================
    // Remove
    // =========================================================================

    auto t_remove = larvae::RegisterTest("HiveBVH", "Remove", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        BVHProxy proxies[5];
        for (int i = 0; i < 5; ++i)
            proxies[i] = bvh.Insert(MakeBox(static_cast<float>(i) * 3.f, 0.f, 0.f));

        bvh.Remove(proxies[1]);
        bvh.Remove(proxies[3]);

        Frustum frustum = MakeLookAtFrustum({6.f, 0.f, 50.f}, {6.f, 0.f, 0.f}, Radians(90.f), 1.f, 0.1f, 200.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 3u);
    });

    // =========================================================================
    // Update
    // =========================================================================

    auto t_update_stays_fat = larvae::RegisterTest("HiveBVH", "UpdateStaysInFatAABB", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        auto proxy = bvh.Insert(MakeBox(0.f, 0.f, 0.f, 1.f));

        bool restructured = bvh.Update(proxy, MakeBox(0.01f, 0.f, 0.f, 1.f));
        larvae::AssertTrue(!restructured);
    });

    auto t_update_exceeds_fat = larvae::RegisterTest("HiveBVH", "UpdateExceedsFatAABB", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        auto proxy = bvh.Insert(MakeBox(0.f, 0.f, 0.f, 0.5f));

        bool restructured = bvh.Update(proxy, MakeBox(100.f, 100.f, 100.f, 0.5f));
        larvae::AssertTrue(restructured);

        Frustum frustum = MakeLookAtFrustum({100.f, 100.f, 110.f}, {100.f, 100.f, 100.f}, Radians(60.f), 1.f, 0.1f, 200.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 1u);
    });

    // =========================================================================
    // Raycast
    // =========================================================================

    auto t_query_ray = larvae::RegisterTest("HiveBVH", "QueryRay", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        AABB boxes[3] = {MakeBox(0.f, 0.f, 0.f), MakeBox(5.f, 0.f, 0.f), MakeBox(10.f, 0.f, 0.f)};
        bvh.Build(boxes, 3);

        uint32_t found = 0;
        bvh.QueryRay({-5.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, 20.f, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 3u);

        found = 0;
        bvh.QueryRay({0.f, -5.f, 0.f}, {0.f, 1.f, 0.f}, 20.f, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 1u);
    });

    // =========================================================================
    // AABB overlap
    // =========================================================================

    auto t_query_aabb = larvae::RegisterTest("HiveBVH", "QueryAABBOverlap", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        AABB boxes[4] = {
            MakeBox(0.f, 0.f, 0.f), MakeBox(3.f, 0.f, 0.f),
            MakeBox(6.f, 0.f, 0.f), MakeBox(9.f, 0.f, 0.f)
        };
        bvh.Build(boxes, 4);

        AABB query{{-1.f, -1.f, -1.f}, {4.f, 1.f, 1.f}};
        uint32_t found = 0;
        bvh.QueryAABB(query, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 2u);
    });

    // =========================================================================
    // Build + dynamic insert mix
    // =========================================================================

    auto t_build_then_insert = larvae::RegisterTest("HiveBVH", "BuildThenInsert", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        AABB boxes[3] = {MakeBox(0.f, 0.f, 0.f), MakeBox(3.f, 0.f, 0.f), MakeBox(6.f, 0.f, 0.f)};
        bvh.Build(boxes, 3);

        (void)bvh.Insert(MakeBox(9.f, 0.f, 0.f));
        (void)bvh.Insert(MakeBox(12.f, 0.f, 0.f));

        larvae::AssertEqual(bvh.ItemCount(), 5u);

        Frustum frustum = MakeLookAtFrustum({6.f, 0.f, 50.f}, {6.f, 0.f, 0.f}, Radians(90.f), 1.f, 0.1f, 200.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t) { found++; });
        larvae::AssertEqual(found, 5u);
    });

    // =========================================================================
    // Refit
    // =========================================================================

    auto t_refit = larvae::RegisterTest("HiveBVH", "Refit", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        AABB boxes[3] = {MakeBox(0.f, 0.f, 0.f), MakeBox(5.f, 0.f, 0.f), MakeBox(10.f, 0.f, 0.f)};
        bvh.Build(boxes, 3);

        bvh.ItemAABB(0) = MakeBox(50.f, 50.f, 50.f);
        bvh.Refit();

        Frustum frustum = MakeLookAtFrustum({50.f, 50.f, 60.f}, {50.f, 50.f, 50.f}, Radians(60.f), 1.f, 0.1f, 100.f);
        uint32_t found = 0;
        bvh.QueryFrustum(frustum, [&](uint32_t idx) {
            if (idx == 0) found++;
        });
        larvae::AssertEqual(found, 1u);

        Frustum old_frustum = MakeLookAtFrustum({0.f, 0.f, 10.f}, {0.f, 0.f, 0.f}, Radians(30.f), 1.f, 0.1f, 20.f);
        found = 0;
        bvh.QueryFrustum(old_frustum, [&](uint32_t idx) {
            if (idx == 0) found++;
        });
        larvae::AssertEqual(found, 0u);
    });

    // =========================================================================
    // Clear
    // =========================================================================

    auto t_clear = larvae::RegisterTest("HiveBVH", "Clear", []() {
        comb::BuddyAllocator alloc{kAllocSize};
        BVH<comb::BuddyAllocator> bvh{alloc};

        AABB boxes[3] = {MakeBox(0.f, 0.f, 0.f), MakeBox(3.f, 0.f, 0.f), MakeBox(6.f, 0.f, 0.f)};
        bvh.Build(boxes, 3);
        larvae::AssertTrue(!bvh.IsEmpty());

        bvh.Clear();
        larvae::AssertTrue(bvh.IsEmpty());
        larvae::AssertEqual(bvh.ItemCount(), 0u);
    });

} // anonymous namespace
