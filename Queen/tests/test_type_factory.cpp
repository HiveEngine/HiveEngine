#include <larvae/larvae.h>
#include <queen/reflect/component_reflector.h>
#include <queen/reflect/reflectable.h>
#include <queen/reflect/component_registry.h>
#include <queen/core/entity.h>
#include <cstring>

namespace
{
    struct Position
    {
        float x = 1.f;
        float y = 2.f;
        float z = 3.f;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("x", &Position::x);
            r.Field("y", &Position::y);
            r.Field("z", &Position::z);
        }
    };

    struct Velocity
    {
        float dx = 0.f;
        float dy = 0.f;
        float dz = 0.f;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("dx", &Velocity::dx);
            r.Field("dy", &Velocity::dy);
            r.Field("dz", &Velocity::dz);
        }
    };

    struct TagComponent {};

    // Nested struct for testing diff on nested fields
    struct Vec2
    {
        float x = 0.f;
        float y = 0.f;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("x", &Vec2::x);
            r.Field("y", &Vec2::y);
        }
    };

    struct Transform
    {
        Vec2 position;
        float rotation = 0.f;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("position", &Transform::position);
            r.Field("rotation", &Transform::rotation);
        }
    };

    // ============================================================
    // Construct tests
    // ============================================================

    auto test_construct_by_type_id = larvae::RegisterTest("QueenTypeFactory", "ConstructByTypeId", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        alignas(Position) unsigned char buffer[sizeof(Position)]{};
        bool ok = registry.Construct(queen::TypeIdOf<Position>(), buffer);

        larvae::AssertTrue(ok);

        auto* pos = reinterpret_cast<Position*>(buffer);
        larvae::AssertEqual(pos->x, 1.f);
        larvae::AssertEqual(pos->y, 2.f);
        larvae::AssertEqual(pos->z, 3.f);
    });

    auto test_construct_not_found = larvae::RegisterTest("QueenTypeFactory", "ConstructNotFound", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        alignas(Velocity) unsigned char buffer[sizeof(Velocity)]{};
        bool ok = registry.Construct(queen::TypeIdOf<Velocity>(), buffer);

        larvae::AssertFalse(ok);
    });

    auto test_construct_zero_default = larvae::RegisterTest("QueenTypeFactory", "ConstructZeroDefault", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Velocity>();

        alignas(Velocity) unsigned char buffer[sizeof(Velocity)];
        std::memset(buffer, 0xFF, sizeof(buffer));

        bool ok = registry.Construct(queen::TypeIdOf<Velocity>(), buffer);
        larvae::AssertTrue(ok);

        auto* vel = reinterpret_cast<Velocity*>(buffer);
        larvae::AssertEqual(vel->dx, 0.f);
        larvae::AssertEqual(vel->dy, 0.f);
        larvae::AssertEqual(vel->dz, 0.f);
    });

    // ============================================================
    // Clone tests
    // ============================================================

    auto test_clone = larvae::RegisterTest("QueenTypeFactory", "CloneByTypeId", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        Position original{10.f, 20.f, 30.f};

        alignas(Position) unsigned char buffer[sizeof(Position)]{};
        bool ok = registry.Clone(queen::TypeIdOf<Position>(), buffer, &original);

        larvae::AssertTrue(ok);

        auto* cloned = reinterpret_cast<Position*>(buffer);
        larvae::AssertEqual(cloned->x, 10.f);
        larvae::AssertEqual(cloned->y, 20.f);
        larvae::AssertEqual(cloned->z, 30.f);
    });

    auto test_clone_not_found = larvae::RegisterTest("QueenTypeFactory", "CloneNotFound", []() {
        queen::ComponentRegistry<32> registry;

        Position src{1.f, 2.f, 3.f};
        alignas(Position) unsigned char buffer[sizeof(Position)]{};

        bool ok = registry.Clone(queen::TypeIdOf<Position>(), buffer, &src);
        larvae::AssertFalse(ok);
    });

    // ============================================================
    // Default value tests
    // ============================================================

    auto test_has_default = larvae::RegisterTest("QueenTypeFactory", "HasDefault", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        const auto* comp = registry.Find(queen::TypeIdOf<Position>());
        larvae::AssertNotNull(comp);
        larvae::AssertTrue(comp->HasDefault());
        larvae::AssertNotNull(comp->default_value);
    });

    auto test_get_default = larvae::RegisterTest("QueenTypeFactory", "GetDefault", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        const void* def = registry.GetDefault(queen::TypeIdOf<Position>());
        larvae::AssertNotNull(def);

        const auto* pos = static_cast<const Position*>(def);
        larvae::AssertEqual(pos->x, 1.f);
        larvae::AssertEqual(pos->y, 2.f);
        larvae::AssertEqual(pos->z, 3.f);
    });

    auto test_get_default_not_found = larvae::RegisterTest("QueenTypeFactory", "GetDefaultNotFound", []() {
        queen::ComponentRegistry<32> registry;

        const void* def = registry.GetDefault(queen::TypeIdOf<Position>());
        larvae::AssertNull(def);
    });

    auto test_without_reflection_has_default = larvae::RegisterTest("QueenTypeFactory", "WithoutReflectionHasDefault", []() {
        queen::ComponentRegistry<32> registry;
        registry.RegisterWithoutReflection<TagComponent>();

        const auto* comp = registry.Find(queen::TypeIdOf<TagComponent>());
        larvae::AssertNotNull(comp);
        larvae::AssertTrue(comp->HasDefault());
    });

    // ============================================================
    // DiffWithDefault tests
    // ============================================================

    auto test_diff_all_default = larvae::RegisterTest("QueenTypeFactory", "DiffAllDefault", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        Position instance{};  // default values: 1, 2, 3
        uint64_t mask = registry.DiffWithDefault(queen::TypeIdOf<Position>(), &instance);

        larvae::AssertEqual(mask, uint64_t{0});
    });

    auto test_diff_one_changed = larvae::RegisterTest("QueenTypeFactory", "DiffOneFieldChanged", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        Position instance{};
        instance.y = 99.f;  // change field index 1

        uint64_t mask = registry.DiffWithDefault(queen::TypeIdOf<Position>(), &instance);

        larvae::AssertEqual(mask, uint64_t{1} << 1);
    });

    auto test_diff_multiple_changed = larvae::RegisterTest("QueenTypeFactory", "DiffMultipleChanged", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        Position instance{};
        instance.x = 0.f;   // changed (default is 1.f)
        instance.z = 0.f;   // changed (default is 3.f)

        uint64_t mask = registry.DiffWithDefault(queen::TypeIdOf<Position>(), &instance);

        // bits 0 and 2 should be set
        larvae::AssertTrue((mask & (uint64_t{1} << 0)) != 0);
        larvae::AssertTrue((mask & (uint64_t{1} << 2)) != 0);
        larvae::AssertTrue((mask & (uint64_t{1} << 1)) == 0);
    });

    auto test_diff_nested_struct = larvae::RegisterTest("QueenTypeFactory", "DiffNestedStruct", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Transform>();

        Transform instance{};
        instance.position.x = 5.f;  // change nested field

        uint64_t mask = registry.DiffWithDefault(queen::TypeIdOf<Transform>(), &instance);

        // position is field 0, rotation is field 1
        // position struct changed -> bit 0 set
        larvae::AssertTrue((mask & (uint64_t{1} << 0)) != 0);
        // rotation unchanged -> bit 1 clear
        larvae::AssertTrue((mask & (uint64_t{1} << 1)) == 0);
    });

    auto test_diff_not_registered = larvae::RegisterTest("QueenTypeFactory", "DiffNotRegistered", []() {
        queen::ComponentRegistry<32> registry;

        Position instance{};
        uint64_t mask = registry.DiffWithDefault(queen::TypeIdOf<Position>(), &instance);

        // Should return all-ones (no info available)
        larvae::AssertEqual(mask, ~uint64_t{0});
    });

    auto test_diff_no_reflection = larvae::RegisterTest("QueenTypeFactory", "DiffNoReflection", []() {
        queen::ComponentRegistry<32> registry;
        registry.RegisterWithoutReflection<TagComponent>();

        TagComponent instance{};
        uint64_t mask = registry.DiffWithDefault(queen::TypeIdOf<TagComponent>(), &instance);

        // No reflection data -> all-ones
        larvae::AssertEqual(mask, ~uint64_t{0});
    });
}
