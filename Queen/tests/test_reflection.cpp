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
}
