#include <larvae/larvae.h>
#include <queen/world/world.h>
#include <comb/linear_allocator.h>

namespace
{
    struct Time
    {
        float elapsed;
        float delta;
    };

    struct Input
    {
        bool up, down, left, right;
    };

    struct Config
    {
        int screen_width;
        int screen_height;
        bool fullscreen;
    };

    struct ResourceWithDestructor
    {
        int* counter;

        ResourceWithDestructor() : counter{nullptr} {}
        ResourceWithDestructor(int* c) : counter{c} {}

        ~ResourceWithDestructor()
        {
            if (counter != nullptr)
            {
                ++(*counter);
            }
        }

        ResourceWithDestructor(const ResourceWithDestructor& other) : counter{other.counter} {}
        ResourceWithDestructor& operator=(const ResourceWithDestructor& other)
        {
            counter = other.counter;
            return *this;
        }

        ResourceWithDestructor(ResourceWithDestructor&& other) noexcept : counter{other.counter}
        {
            other.counter = nullptr;
        }
        ResourceWithDestructor& operator=(ResourceWithDestructor&& other) noexcept
        {
            counter = other.counter;
            other.counter = nullptr;
            return *this;
        }
    };

    auto test1 = larvae::RegisterTest("QueenResource", "InsertAndGet", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        Time* time = world.Resource<Time>();
        larvae::AssertNotNull(time);
        larvae::AssertEqual(time->elapsed, 0.0f);
        larvae::AssertEqual(time->delta, 0.016f);
    });

    auto test2 = larvae::RegisterTest("QueenResource", "HasResource", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        larvae::AssertFalse(world.HasResource<Time>());

        world.InsertResource(Time{0.0f, 0.016f});

        larvae::AssertTrue(world.HasResource<Time>());
        larvae::AssertFalse(world.HasResource<Input>());
    });

    auto test3 = larvae::RegisterTest("QueenResource", "RemoveResource", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});
        larvae::AssertTrue(world.HasResource<Time>());
        larvae::AssertEqual(world.ResourceCount(), size_t{1});

        world.RemoveResource<Time>();

        larvae::AssertFalse(world.HasResource<Time>());
        larvae::AssertNull(world.Resource<Time>());
        larvae::AssertEqual(world.ResourceCount(), size_t{0});
    });

    auto test4 = larvae::RegisterTest("QueenResource", "MultipleResources", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        world.InsertResource(Time{1.0f, 0.016f});
        world.InsertResource(Input{true, false, true, false});
        world.InsertResource(Config{1920, 1080, true});

        larvae::AssertEqual(world.ResourceCount(), size_t{3});

        Time* time = world.Resource<Time>();
        Input* input = world.Resource<Input>();
        Config* config = world.Resource<Config>();

        larvae::AssertNotNull(time);
        larvae::AssertNotNull(input);
        larvae::AssertNotNull(config);

        larvae::AssertEqual(time->elapsed, 1.0f);
        larvae::AssertTrue(input->up);
        larvae::AssertFalse(input->down);
        larvae::AssertEqual(config->screen_width, 1920);
        larvae::AssertTrue(config->fullscreen);
    });

    auto test5 = larvae::RegisterTest("QueenResource", "UpdateExistingResource", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        Time* time1 = world.Resource<Time>();
        larvae::AssertEqual(time1->elapsed, 0.0f);

        world.InsertResource(Time{1.0f, 0.033f});

        Time* time2 = world.Resource<Time>();
        larvae::AssertEqual(time2->elapsed, 1.0f);
        larvae::AssertEqual(time2->delta, 0.033f);

        larvae::AssertEqual(world.ResourceCount(), size_t{1});
    });

    auto test6 = larvae::RegisterTest("QueenResource", "ModifyResource", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        Time* time = world.Resource<Time>();
        time->elapsed += time->delta;
        time->elapsed += time->delta;
        time->elapsed += time->delta;

        Time* timeAfter = world.Resource<Time>();
        larvae::AssertTrue(timeAfter->elapsed > 0.04f);
    });

    auto test7 = larvae::RegisterTest("QueenResource", "ConstAccess", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};
        world.InsertResource(Time{5.0f, 0.016f});

        const auto& const_world = world;

        const Time* time = const_world.Resource<Time>();
        larvae::AssertNotNull(time);
        larvae::AssertEqual(time->elapsed, 5.0f);

        larvae::AssertTrue(const_world.HasResource<Time>());
    });

    auto test8 = larvae::RegisterTest("QueenResource", "GetNonExistent", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        Time* time = world.Resource<Time>();
        larvae::AssertNull(time);
    });

    auto test9 = larvae::RegisterTest("QueenResource", "RemoveNonExistent", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        world.RemoveResource<Time>();

        larvae::AssertFalse(world.HasResource<Time>());
        larvae::AssertEqual(world.ResourceCount(), size_t{0});
    });

    auto test10 = larvae::RegisterTest("QueenResource", "DestructorCalled", []() {
        int destruct_count = 0;

        {
            comb::LinearAllocator alloc{65536};
            queen::World world{};

            world.InsertResource(ResourceWithDestructor{&destruct_count});
        }

        larvae::AssertGreaterEqual(destruct_count, 1);
    });

    auto test11 = larvae::RegisterTest("QueenResource", "DestructorCalledOnRemove", []() {
        int destruct_count = 0;

        comb::LinearAllocator alloc{65536};
        queen::World world{};

        world.InsertResource(ResourceWithDestructor{&destruct_count});

        int count_before = destruct_count;
        world.RemoveResource<ResourceWithDestructor>();

        larvae::AssertGreaterThan(destruct_count, count_before);
    });

    auto test12 = larvae::RegisterTest("QueenResource", "ResourcesWithEntities", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        struct Position { float x, y, z; };
        struct Velocity { float dx, dy, dz; };

        queen::Entity e1 = world.Spawn(Position{0.0f, 0.0f, 0.0f}, Velocity{1.0f, 0.0f, 0.0f});
        queen::Entity e2 = world.Spawn(Position{10.0f, 0.0f, 0.0f}, Velocity{-1.0f, 0.0f, 0.0f});

        larvae::AssertEqual(world.EntityCount(), size_t{2});
        larvae::AssertEqual(world.ResourceCount(), size_t{1});

        Time* time = world.Resource<Time>();
        Position* pos1 = world.Get<Position>(e1);
        Velocity* vel1 = world.Get<Velocity>(e1);

        pos1->x += vel1->dx * time->delta;

        larvae::AssertTrue(pos1->x > 0.0f);

        world.Despawn(e1);
        world.Despawn(e2);

        larvae::AssertEqual(world.EntityCount(), size_t{0});
        larvae::AssertEqual(world.ResourceCount(), size_t{1});
    });
}
