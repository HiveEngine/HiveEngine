#include <larvae/larvae.h>
#include <wax/pointers/handle.h>
#include <comb/linear_allocator.h>

namespace
{
    struct Entity
    {
        int id;
        float x, y;

        Entity() : id{0}, x{0}, y{0} {}
        Entity(int i, float px, float py) : id{i}, x{px}, y{py} {}
    };

    auto test1 = larvae::RegisterTest("WaxHandle", "CreateAndGet", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 10};

        auto handle = pool.Create(42, 1.0f, 2.0f);

        larvae::AssertFalse(handle.IsNull());
        larvae::AssertTrue(pool.IsValid(handle));

        Entity* entity = pool.Get(handle);
        larvae::AssertNotNull(entity);
        larvae::AssertEqual(entity->id, 42);
        larvae::AssertEqual(entity->x, 1.0f);
        larvae::AssertEqual(entity->y, 2.0f);
    });

    auto test2 = larvae::RegisterTest("WaxHandle", "DestroyInvalidatesHandle", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 10};

        auto handle = pool.Create(1, 0.0f, 0.0f);
        larvae::AssertTrue(pool.IsValid(handle));
        larvae::AssertEqual(pool.Count(), size_t{1});

        pool.Destroy(handle);

        larvae::AssertFalse(pool.IsValid(handle));
        larvae::AssertNull(pool.Get(handle));
        larvae::AssertEqual(pool.Count(), size_t{0});
    });

    auto test3 = larvae::RegisterTest("WaxHandle", "GenerationPreventsUseAfterFree", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 10};

        auto handle1 = pool.Create(1, 0.0f, 0.0f);
        pool.Destroy(handle1);

        auto handle2 = pool.Create(2, 0.0f, 0.0f);

        larvae::AssertFalse(pool.IsValid(handle1));
        larvae::AssertTrue(pool.IsValid(handle2));

        larvae::AssertNull(pool.Get(handle1));
        larvae::AssertNotNull(pool.Get(handle2));
        larvae::AssertEqual(pool.Get(handle2)->id, 2);
    });

    auto test4 = larvae::RegisterTest("WaxHandle", "SlotReuse", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 2};

        auto h1 = pool.Create(1, 0.0f, 0.0f);
        [[maybe_unused]] auto h2 = pool.Create(2, 0.0f, 0.0f);

        larvae::AssertTrue(pool.IsFull());

        auto h3 = pool.Create(3, 0.0f, 0.0f);
        larvae::AssertTrue(h3.IsNull());

        pool.Destroy(h1);
        larvae::AssertFalse(pool.IsFull());

        auto h4 = pool.Create(4, 0.0f, 0.0f);
        larvae::AssertFalse(h4.IsNull());
        larvae::AssertEqual(pool.Get(h4)->id, 4);

        larvae::AssertEqual(h4.index, h1.index);
        larvae::AssertNotEqual(h4.generation, h1.generation);
    });

    auto test5 = larvae::RegisterTest("WaxHandle", "InvalidHandle", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 10};

        auto invalid = wax::Handle<Entity>::Invalid();

        larvae::AssertTrue(invalid.IsNull());
        larvae::AssertFalse(pool.IsValid(invalid));
        larvae::AssertNull(pool.Get(invalid));
    });

    auto test6 = larvae::RegisterTest("WaxHandle", "MultipleCreateDestroy", []() {
        comb::LinearAllocator alloc{8192};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 100};

        wax::Handle<Entity> handles[50];

        for (int i = 0; i < 50; ++i)
        {
            handles[i] = pool.Create(i, static_cast<float>(i), 0.0f);
        }

        larvae::AssertEqual(pool.Count(), size_t{50});

        for (int i = 0; i < 25; ++i)
        {
            pool.Destroy(handles[i]);
        }

        larvae::AssertEqual(pool.Count(), size_t{25});

        for (int i = 0; i < 25; ++i)
        {
            larvae::AssertFalse(pool.IsValid(handles[i]));
        }

        for (int i = 25; i < 50; ++i)
        {
            larvae::AssertTrue(pool.IsValid(handles[i]));
            larvae::AssertEqual(pool.Get(handles[i])->id, i);
        }
    });

    auto test7 = larvae::RegisterTest("WaxHandle", "HandleEquality", []() {
        wax::Handle<Entity> h1{5, 10};
        wax::Handle<Entity> h2{5, 10};
        wax::Handle<Entity> h3{5, 11};
        wax::Handle<Entity> h4{6, 10};

        larvae::AssertTrue(h1 == h2);
        larvae::AssertFalse(h1 != h2);
        larvae::AssertFalse(h1 == h3);
        larvae::AssertFalse(h1 == h4);
    });

    auto test8 = larvae::RegisterTest("WaxHandle", "ConstGet", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 10};

        auto handle = pool.Create(42, 1.0f, 2.0f);

        const auto& constPool = pool;
        const Entity* entity = constPool.Get(handle);

        larvae::AssertNotNull(entity);
        larvae::AssertEqual(entity->id, 42);
    });

    struct NonTrivial
    {
        static int destructor_count;
        int value;

        NonTrivial(int v) : value{v} {}
        ~NonTrivial() { ++destructor_count; }
    };

    int NonTrivial::destructor_count = 0;

    auto test9 = larvae::RegisterTest("WaxHandle", "DestructorCalled", []() {
        NonTrivial::destructor_count = 0;

        {
            comb::LinearAllocator alloc{4096};
            wax::HandlePool<NonTrivial, comb::LinearAllocator> pool{alloc, 10};

            auto h1 = pool.Create(1);
            auto h2 = pool.Create(2);
            [[maybe_unused]] auto h3 = pool.Create(3);

            pool.Destroy(h1);
            larvae::AssertEqual(NonTrivial::destructor_count, 1);

            pool.Destroy(h2);
            larvae::AssertEqual(NonTrivial::destructor_count, 2);
        }

        larvae::AssertEqual(NonTrivial::destructor_count, 3);
    });

    auto test10 = larvae::RegisterTest("WaxHandle", "MoveConstructPool", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool1{alloc, 10};

        auto h1 = pool1.Create(1, 0.0f, 0.0f);
        auto h2 = pool1.Create(2, 0.0f, 0.0f);

        wax::HandlePool<Entity, comb::LinearAllocator> pool2{std::move(pool1)};

        larvae::AssertEqual(pool2.Count(), size_t{2});
        larvae::AssertTrue(pool2.IsValid(h1));
        larvae::AssertTrue(pool2.IsValid(h2));
        larvae::AssertEqual(pool2.Get(h1)->id, 1);
        larvae::AssertEqual(pool2.Get(h2)->id, 2);
    });

    auto test11 = larvae::RegisterTest("WaxHandle", "MoveAssignPool", []() {
        comb::LinearAllocator alloc{8192};
        wax::HandlePool<Entity, comb::LinearAllocator> pool1{alloc, 10};
        wax::HandlePool<Entity, comb::LinearAllocator> pool2{alloc, 5};

        auto h1 = pool1.Create(1, 0.0f, 0.0f);
        [[maybe_unused]] auto h_old = pool2.Create(99, 0.0f, 0.0f);

        pool2 = std::move(pool1);

        larvae::AssertEqual(pool2.Count(), size_t{1});
        larvae::AssertTrue(pool2.IsValid(h1));
        larvae::AssertEqual(pool2.Get(h1)->id, 1);
    });

    auto test12 = larvae::RegisterTest("WaxHandle", "PoolExhaustion", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 3};

        auto h1 = pool.Create(1, 0.0f, 0.0f);
        auto h2 = pool.Create(2, 0.0f, 0.0f);
        auto h3 = pool.Create(3, 0.0f, 0.0f);

        larvae::AssertFalse(h1.IsNull());
        larvae::AssertFalse(h2.IsNull());
        larvae::AssertFalse(h3.IsNull());
        larvae::AssertTrue(pool.IsFull());
        larvae::AssertEqual(pool.Count(), size_t{3});

        // Pool full: returns invalid handle
        auto h4 = pool.Create(4, 0.0f, 0.0f);
        larvae::AssertTrue(h4.IsNull());

        // Destroy one, create another
        pool.Destroy(h2);
        larvae::AssertFalse(pool.IsFull());

        auto h5 = pool.Create(5, 0.0f, 0.0f);
        larvae::AssertFalse(h5.IsNull());
        larvae::AssertEqual(pool.Get(h5)->id, 5);
    });

    auto test13 = larvae::RegisterTest("WaxHandle", "EmptyPool", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 10};

        larvae::AssertTrue(pool.IsEmpty());
        larvae::AssertEqual(pool.Count(), size_t{0});
        larvae::AssertEqual(pool.Capacity(), size_t{10});
        larvae::AssertFalse(pool.IsFull());
    });

    auto test14 = larvae::RegisterTest("WaxHandle", "DestroyInvalidHandleNoOp", []() {
        comb::LinearAllocator alloc{4096};
        wax::HandlePool<Entity, comb::LinearAllocator> pool{alloc, 10};

        // Destroy invalid handle - should be no-op
        pool.Destroy(wax::Handle<Entity>::Invalid());
        larvae::AssertEqual(pool.Count(), size_t{0});

        // Destroy handle with wrong generation
        auto h = pool.Create(1, 0.0f, 0.0f);
        pool.Destroy(h);

        // Double-destroy same handle - should be no-op
        pool.Destroy(h);
        larvae::AssertEqual(pool.Count(), size_t{0});
    });
}
