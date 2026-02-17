#include <larvae/larvae.h>
#include <queen/reflect/component_reflector.h>
#include <queen/reflect/reflectable.h>
#include <queen/reflect/component_serializer.h>
#include <queen/reflect/component_registry.h>
#include <queen/core/entity.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/binary_reader.h>
#include <comb/linear_allocator.h>

namespace
{
    // Test components with reflection
    // Components use ComponentReflector<> with default size (32)
    struct Position
    {
        float x, y, z;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("x", &Position::x);
            r.Field("y", &Position::y);
            r.Field("z", &Position::z);
        }
    };

    struct Velocity
    {
        float dx, dy, dz;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("dx", &Velocity::dx);
            r.Field("dy", &Velocity::dy);
            r.Field("dz", &Velocity::dz);
        }
    };

    struct Health
    {
        int32_t current;
        int32_t maximum;
        bool is_dead;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("current", &Health::current);
            r.Field("maximum", &Health::maximum);
            r.Field("is_dead", &Health::is_dead);
        }
    };

    struct AllTypes
    {
        int8_t i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        float f32;
        double f64;
        bool flag;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("i8", &AllTypes::i8);
            r.Field("i16", &AllTypes::i16);
            r.Field("i32", &AllTypes::i32);
            r.Field("i64", &AllTypes::i64);
            r.Field("u8", &AllTypes::u8);
            r.Field("u16", &AllTypes::u16);
            r.Field("u32", &AllTypes::u32);
            r.Field("u64", &AllTypes::u64);
            r.Field("f32", &AllTypes::f32);
            r.Field("f64", &AllTypes::f64);
            r.Field("flag", &AllTypes::flag);
        }
    };

    struct WithEntity
    {
        queen::Entity target;
        int32_t data;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("target", &WithEntity::target);
            r.Field("data", &WithEntity::data);
        }
    };

    // Non-reflectable component for testing
    struct TagComponent {};

    // Nested reflectable struct
    struct Vec2
    {
        float x, y;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("x", &Vec2::x);
            r.Field("y", &Vec2::y);
        }
    };

    // Component containing a nested reflectable struct
    struct Transform
    {
        Vec2 position;
        Vec2 scale;
        float rotation;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("position", &Transform::position);
            r.Field("scale", &Transform::scale);
            r.Field("rotation", &Transform::rotation);
        }
    };

    // Nested struct containing an Entity field
    struct TargetInfo
    {
        queen::Entity entity;
        int32_t priority;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("entity", &TargetInfo::entity);
            r.Field("priority", &TargetInfo::priority);
        }
    };

    // Component with nested struct that has an Entity
    struct AIComponent
    {
        TargetInfo primary_target;
        TargetInfo secondary_target;
        float aggro_range;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("primary_target", &AIComponent::primary_target);
            r.Field("secondary_target", &AIComponent::secondary_target);
            r.Field("aggro_range", &AIComponent::aggro_range);
        }
    };

    // Non-reflectable nested struct (no Reflect method)
    struct Color
    {
        uint8_t r, g, b, a;
    };

    // Component containing a non-reflectable nested struct
    struct Sprite
    {
        Color tint;
        float opacity;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("tint", &Sprite::tint);
            r.Field("opacity", &Sprite::opacity);
        }
    };

    // ============================================================
    // ComponentReflector tests
    // ============================================================

    auto test1 = larvae::RegisterTest("QueenReflection", "ComponentReflectorBasic", []() {
        queen::ComponentReflector<> reflector;
        Position::Reflect(reflector);

        larvae::AssertEqual(reflector.Count(), size_t{3});
    });

    auto test2 = larvae::RegisterTest("QueenReflection", "FieldInfoCorrect", []() {
        queen::ComponentReflector<> reflector;
        Position::Reflect(reflector);

        const auto& field_x = reflector[0];
        larvae::AssertTrue(field_x.name != nullptr);
        larvae::AssertEqual(field_x.offset, size_t{0});
        larvae::AssertEqual(field_x.size, sizeof(float));
        larvae::AssertEqual(static_cast<int>(field_x.type), static_cast<int>(queen::FieldType::Float32));
    });

    auto test3 = larvae::RegisterTest("QueenReflection", "FieldOffsets", []() {
        queen::ComponentReflector<> reflector;
        Position::Reflect(reflector);

        larvae::AssertEqual(reflector[0].offset, offsetof(Position, x));
        larvae::AssertEqual(reflector[1].offset, offsetof(Position, y));
        larvae::AssertEqual(reflector[2].offset, offsetof(Position, z));
    });

    auto test4 = larvae::RegisterTest("QueenReflection", "FindFieldByName", []() {
        queen::ComponentReflector<> reflector;
        Position::Reflect(reflector);

        const queen::FieldInfo* field = reflector.FindField("y");
        larvae::AssertNotNull(field);
        larvae::AssertEqual(field->offset, offsetof(Position, y));
    });

    auto test5 = larvae::RegisterTest("QueenReflection", "FindFieldNotFound", []() {
        queen::ComponentReflector<> reflector;
        Position::Reflect(reflector);

        const queen::FieldInfo* field = reflector.FindField("w");
        larvae::AssertNull(field);
    });

    // ============================================================
    // Reflectable concept tests
    // ============================================================

    auto test6 = larvae::RegisterTest("QueenReflection", "ReflectableConceptPositive", []() {
        larvae::AssertTrue(queen::Reflectable<Position>);
        larvae::AssertTrue(queen::Reflectable<Velocity>);
        larvae::AssertTrue(queen::Reflectable<Health>);
    });

    auto test7 = larvae::RegisterTest("QueenReflection", "ReflectableConceptNegative", []() {
        larvae::AssertFalse(queen::Reflectable<TagComponent>);
        larvae::AssertFalse(queen::Reflectable<int>);
    });

    auto test8 = larvae::RegisterTest("QueenReflection", "GetReflectionDataValid", []() {
        auto reflection = queen::GetReflectionData<Position>();

        larvae::AssertTrue(reflection.IsValid());
        larvae::AssertEqual(reflection.field_count, size_t{3});
        larvae::AssertNotNull(reflection.fields);
        larvae::AssertEqual(reflection.type_id, queen::TypeIdOf<Position>());
    });

    // ============================================================
    // Serialization tests
    // ============================================================

    auto test9 = larvae::RegisterTest("QueenReflection", "SerializeDeserializePosition", []() {
        Position original{1.0f, 2.0f, 3.0f};

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::Serialize(original, writer);

        Position loaded{0.0f, 0.0f, 0.0f};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(loaded.x, 1.0f);
        larvae::AssertEqual(loaded.y, 2.0f);
        larvae::AssertEqual(loaded.z, 3.0f);
    });

    auto test10 = larvae::RegisterTest("QueenReflection", "SerializeDeserializeHealth", []() {
        Health original{80, 100, false};

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::Serialize(original, writer);

        Health loaded{0, 0, true};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(loaded.current, int32_t{80});
        larvae::AssertEqual(loaded.maximum, int32_t{100});
        larvae::AssertFalse(loaded.is_dead);
    });

    auto test11 = larvae::RegisterTest("QueenReflection", "SerializeDeserializeAllTypes", []() {
        AllTypes original{
            -8, -16, -32, -64,
            8, 16, 32, 64,
            3.14f, 2.718281828,
            true
        };

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::Serialize(original, writer);

        AllTypes loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(loaded.i8, int8_t{-8});
        larvae::AssertEqual(loaded.i16, int16_t{-16});
        larvae::AssertEqual(loaded.i32, int32_t{-32});
        larvae::AssertEqual(loaded.i64, int64_t{-64});
        larvae::AssertEqual(loaded.u8, uint8_t{8});
        larvae::AssertEqual(loaded.u16, uint16_t{16});
        larvae::AssertEqual(loaded.u32, uint32_t{32});
        larvae::AssertEqual(loaded.u64, uint64_t{64});
        larvae::AssertEqual(loaded.f32, 3.14f);
        larvae::AssertEqual(loaded.f64, 2.718281828);
        larvae::AssertTrue(loaded.flag);
    });

    auto test12 = larvae::RegisterTest("QueenReflection", "SerializeDeserializeEntity", []() {
        WithEntity original{
            queen::Entity{42, 7, queen::Entity::Flags::kAlive},
            12345
        };

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::Serialize(original, writer);

        WithEntity loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(loaded.target.Index(), queen::Entity::IndexType{42});
        larvae::AssertEqual(loaded.target.Generation(), queen::Entity::GenerationType{7});
        larvae::AssertEqual(loaded.data, int32_t{12345});
    });

    // ============================================================
    // ComponentRegistry tests
    // ============================================================

    auto test13 = larvae::RegisterTest("QueenReflection", "RegistryRegisterFind", []() {
        queen::ComponentRegistry<32> registry;

        registry.Register<Position>();
        registry.Register<Velocity>();

        larvae::AssertEqual(registry.Count(), size_t{2});
        larvae::AssertTrue(registry.Contains<Position>());
        larvae::AssertTrue(registry.Contains<Velocity>());
    });

    auto test14 = larvae::RegisterTest("QueenReflection", "RegistryFindByTypeId", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        const queen::RegisteredComponent* found = registry.Find(queen::TypeIdOf<Position>());

        larvae::AssertNotNull(found);
        larvae::AssertTrue(found->IsValid());
        larvae::AssertTrue(found->HasReflection());
        larvae::AssertEqual(found->meta.type_id, queen::TypeIdOf<Position>());
    });

    auto test15 = larvae::RegisterTest("QueenReflection", "RegistryFindNotRegistered", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        const queen::RegisteredComponent* found = registry.Find(queen::TypeIdOf<Velocity>());

        larvae::AssertNull(found);
    });

    auto test16 = larvae::RegisterTest("QueenReflection", "RegistryRegisterWithoutReflection", []() {
        queen::ComponentRegistry<32> registry;
        registry.RegisterWithoutReflection<TagComponent>();

        larvae::AssertTrue(registry.Contains<TagComponent>());

        const queen::RegisteredComponent* found = registry.Find(queen::TypeIdOf<TagComponent>());
        larvae::AssertNotNull(found);
        larvae::AssertTrue(found->IsValid());
        larvae::AssertFalse(found->HasReflection());
    });

    auto test17 = larvae::RegisterTest("QueenReflection", "RegistryIterator", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();
        registry.Register<Velocity>();
        registry.Register<Health>();

        size_t count = 0;
        for (const auto& entry : registry)
        {
            larvae::AssertTrue(entry.IsValid());
            ++count;
        }

        larvae::AssertEqual(count, size_t{3});
    });

    auto test18 = larvae::RegisterTest("QueenReflection", "SerializeUsingReflection", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        Position original{5.0f, 10.0f, 15.0f};

        const queen::RegisteredComponent* info = registry.Find(queen::TypeIdOf<Position>());
        larvae::AssertNotNull(info);

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::SerializeComponent(&original, info->reflection, writer);

        Position loaded{0.0f, 0.0f, 0.0f};
        wax::BinaryReader reader{writer.View()};
        queen::DeserializeComponent(&loaded, info->reflection, reader);

        larvae::AssertEqual(loaded.x, 5.0f);
        larvae::AssertEqual(loaded.y, 10.0f);
        larvae::AssertEqual(loaded.z, 15.0f);
    });

    // ============================================================
    // Nested struct serialization tests
    // ============================================================

    auto test19 = larvae::RegisterTest("QueenReflection", "NestedReflectableFieldInfo", []() {
        queen::ComponentReflector<> reflector;
        Transform::Reflect(reflector);

        // 3 fields: position (Vec2), scale (Vec2), rotation (float)
        larvae::AssertEqual(reflector.Count(), size_t{3});

        const auto& pos_field = reflector[0];
        larvae::AssertEqual(static_cast<int>(pos_field.type), static_cast<int>(queen::FieldType::Struct));
        larvae::AssertEqual(pos_field.size, sizeof(Vec2));
        // Nested reflectable should have nested field pointers
        larvae::AssertNotNull(pos_field.nested_fields);
        larvae::AssertEqual(pos_field.nested_field_count, size_t{2});
    });

    auto test20 = larvae::RegisterTest("QueenReflection", "NestedReflectableSerializeRoundtrip", []() {
        Transform original{};
        original.position = {1.0f, 2.0f};
        original.scale = {3.0f, 4.0f};
        original.rotation = 90.0f;

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::Serialize(original, writer);

        Transform loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(loaded.position.x, 1.0f);
        larvae::AssertEqual(loaded.position.y, 2.0f);
        larvae::AssertEqual(loaded.scale.x, 3.0f);
        larvae::AssertEqual(loaded.scale.y, 4.0f);
        larvae::AssertEqual(loaded.rotation, 90.0f);
    });

    auto test21 = larvae::RegisterTest("QueenReflection", "NestedStructWithEntityRoundtrip", []() {
        AIComponent original{};
        original.primary_target.entity = queen::Entity{100, 5, queen::Entity::Flags::kAlive};
        original.primary_target.priority = 10;
        original.secondary_target.entity = queen::Entity{200, 3, queen::Entity::Flags::kAlive};
        original.secondary_target.priority = 5;
        original.aggro_range = 50.0f;

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::Serialize(original, writer);

        AIComponent loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        // Primary target entity
        larvae::AssertEqual(loaded.primary_target.entity.Index(), queen::Entity::IndexType{100});
        larvae::AssertEqual(loaded.primary_target.entity.Generation(), queen::Entity::GenerationType{5});
        larvae::AssertEqual(loaded.primary_target.priority, int32_t{10});
        // Secondary target entity
        larvae::AssertEqual(loaded.secondary_target.entity.Index(), queen::Entity::IndexType{200});
        larvae::AssertEqual(loaded.secondary_target.entity.Generation(), queen::Entity::GenerationType{3});
        larvae::AssertEqual(loaded.secondary_target.priority, int32_t{5});
        // Flat field
        larvae::AssertEqual(loaded.aggro_range, 50.0f);
    });

    auto test22 = larvae::RegisterTest("QueenReflection", "NonReflectableNestedStructFallback", []() {
        // Color has no Reflect() method — serializer should fall back to raw bytes
        Sprite original{};
        original.tint = {255, 128, 0, 200};
        original.opacity = 0.75f;

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::Serialize(original, writer);

        Sprite loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(loaded.tint.r, uint8_t{255});
        larvae::AssertEqual(loaded.tint.g, uint8_t{128});
        larvae::AssertEqual(loaded.tint.b, uint8_t{0});
        larvae::AssertEqual(loaded.tint.a, uint8_t{200});
        larvae::AssertEqual(loaded.opacity, 0.75f);
    });

    auto test23 = larvae::RegisterTest("QueenReflection", "NonReflectableNestedFieldInfo", []() {
        queen::ComponentReflector<> reflector;
        Sprite::Reflect(reflector);

        const auto& tint_field = reflector[0];
        larvae::AssertEqual(static_cast<int>(tint_field.type), static_cast<int>(queen::FieldType::Struct));
        // Non-reflectable nested type should have null nested_fields
        larvae::AssertNull(tint_field.nested_fields);
        larvae::AssertEqual(tint_field.nested_field_count, size_t{0});
    });

    auto test24 = larvae::RegisterTest("QueenReflection", "NestedReflectableViaRegistry", []() {
        // Test nested struct serialization going through the registry path
        queen::ComponentRegistry<32> registry;
        registry.Register<AIComponent>();

        const queen::RegisteredComponent* info = registry.Find(queen::TypeIdOf<AIComponent>());
        larvae::AssertNotNull(info);
        larvae::AssertTrue(info->HasReflection());

        AIComponent original{};
        original.primary_target.entity = queen::Entity{42, 1, queen::Entity::Flags::kAlive};
        original.primary_target.priority = 99;
        original.secondary_target.entity = queen::Entity{0, 0, {}};
        original.secondary_target.priority = 0;
        original.aggro_range = 25.0f;

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

        queen::SerializeComponent(&original, info->reflection, writer);

        AIComponent loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::DeserializeComponent(&loaded, info->reflection, reader);

        larvae::AssertEqual(loaded.primary_target.entity.Index(), queen::Entity::IndexType{42});
        larvae::AssertEqual(loaded.primary_target.priority, int32_t{99});
        larvae::AssertEqual(loaded.aggro_range, 25.0f);
    });

    // ============================================================
    // Registry FindByName tests
    // ============================================================

    auto test25 = larvae::RegisterTest("QueenReflection", "RegistryFindByNameFound", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();
        registry.Register<Velocity>();
        registry.Register<Health>();

        // GetReflectionData returns name from TypeNameOf<T>()
        auto reflection = queen::GetReflectionData<Position>();
        const queen::RegisteredComponent* found = registry.FindByName(reflection.name);

        larvae::AssertNotNull(found);
        larvae::AssertEqual(found->meta.type_id, queen::TypeIdOf<Position>());
    });

    auto test26 = larvae::RegisterTest("QueenReflection", "RegistryFindByNameNotFound", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        const queen::RegisteredComponent* found = registry.FindByName("NonExistentComponent");
        larvae::AssertNull(found);
    });

    auto test27 = larvae::RegisterTest("QueenReflection", "RegistryFindByNameNull", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Position>();

        const queen::RegisteredComponent* found = registry.FindByName(nullptr);
        larvae::AssertNull(found);
    });

    auto test28 = larvae::RegisterTest("QueenReflection", "RegistryFindByNameNoReflection", []() {
        // RegisterWithoutReflection stores no name, so FindByName shouldn't find it
        queen::ComponentRegistry<32> registry;
        registry.RegisterWithoutReflection<TagComponent>();

        // With no reflection data, name is nullptr, so any name search should not crash
        const queen::RegisteredComponent* found = registry.FindByName("TagComponent");
        larvae::AssertNull(found);
    });
}
