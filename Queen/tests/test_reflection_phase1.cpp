#include <larvae/larvae.h>
#include <queen/reflect/component_reflector.h>
#include <queen/reflect/reflectable.h>
#include <queen/reflect/component_serializer.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/enum_reflection.h>
#include <queen/reflect/field_attributes.h>
#include <queen/core/entity.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/binary_reader.h>
#include <wax/containers/fixed_string.h>
#include <comb/linear_allocator.h>

namespace
{
    // ============================================================
    // Test enums
    // ============================================================

    enum class RenderMode : uint8_t
    {
        Opaque = 0,
        Transparent = 1,
        Wireframe = 2,
    };

    enum class Alignment : int32_t
    {
        Left = -1,
        Center = 0,
        Right = 1,
    };

    // Enum without reflection (no EnumInfo specialization)
    enum class InternalFlag : uint8_t
    {
        Off = 0,
        On = 1,
    };
}

// EnumInfo specializations (outside anonymous namespace for template specialization)
template<> struct queen::EnumInfo<RenderMode>
{
    static const queen::EnumReflectionBase& Get()
    {
        static auto r = []() {
            queen::EnumReflector<> e;
            e.Value("Opaque", RenderMode::Opaque);
            e.Value("Transparent", RenderMode::Transparent);
            e.Value("Wireframe", RenderMode::Wireframe);
            return e;
        }();
        return r.Base();
    }
};

template<> struct queen::EnumInfo<Alignment>
{
    static const queen::EnumReflectionBase& Get()
    {
        static auto r = []() {
            queen::EnumReflector<> e;
            e.Value("Left", Alignment::Left);
            e.Value("Center", Alignment::Center);
            e.Value("Right", Alignment::Right);
            return e;
        }();
        return r.Base();
    }
};

namespace
{
    // ============================================================
    // Test components
    // ============================================================

    struct WithEnum
    {
        RenderMode mode;
        float alpha;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("mode", &WithEnum::mode);
            r.Field("alpha", &WithEnum::alpha);
        }
    };

    struct WithAlignment
    {
        Alignment align;
        int32_t padding;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("align", &WithAlignment::align);
            r.Field("padding", &WithAlignment::padding);
        }
    };

    struct WithUnreflectedEnum
    {
        InternalFlag flag;
        uint8_t data;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("flag", &WithUnreflectedEnum::flag);
            r.Field("data", &WithUnreflectedEnum::data);
        }
    };

    struct WithFixedString
    {
        wax::FixedString name;
        int32_t id;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("name", &WithFixedString::name);
            r.Field("id", &WithFixedString::id);
        }
    };

    struct WithFixedArray
    {
        float values[4];
        int32_t count;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("values", &WithFixedArray::values);
            r.Field("count", &WithFixedArray::count);
        }
    };

    struct WithAnnotations
    {
        float speed;
        float health;
        float rotation;
        RenderMode mode;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("speed", &WithAnnotations::speed)
                .Range(0.f, 100.f, 0.5f)
                .Tooltip("Movement speed in units/sec")
                .Category("Movement");
            r.Field("health", &WithAnnotations::health)
                .Range(0.f, 1000.f)
                .Flag(queen::FieldFlag::ReadOnly);
            r.Field("rotation", &WithAnnotations::rotation)
                .Flag(queen::FieldFlag::Angle)
                .DisplayName("Rotation (deg)");
            r.Field("mode", &WithAnnotations::mode);
        }
    };

    // ============================================================
    // Enum reflection tests
    // ============================================================

    auto test_enum_concept = larvae::RegisterTest("QueenReflectionPhase1", "ReflectableEnumConcept", []() {
        larvae::AssertTrue(queen::ReflectableEnum<RenderMode>);
        larvae::AssertTrue(queen::ReflectableEnum<Alignment>);
        larvae::AssertFalse(queen::ReflectableEnum<InternalFlag>);
        larvae::AssertFalse(queen::ReflectableEnum<int>);
    });

    auto test_enum_name_of = larvae::RegisterTest("QueenReflectionPhase1", "EnumNameOf", []() {
        const auto& info = queen::EnumInfo<RenderMode>::Get();

        larvae::AssertTrue(info.IsValid());
        larvae::AssertEqual(info.entry_count, size_t{3});
        larvae::AssertEqual(info.underlying_size, sizeof(uint8_t));

        const char* name = info.NameOf(0);
        larvae::AssertNotNull(name);
        larvae::AssertTrue(queen::detail::StringsEqual(name, "Opaque"));

        name = info.NameOf(2);
        larvae::AssertNotNull(name);
        larvae::AssertTrue(queen::detail::StringsEqual(name, "Wireframe"));

        larvae::AssertNull(info.NameOf(99));
    });

    auto test_enum_value_of = larvae::RegisterTest("QueenReflectionPhase1", "EnumValueOf", []() {
        const auto& info = queen::EnumInfo<RenderMode>::Get();

        int64_t value = -1;
        larvae::AssertTrue(info.ValueOf("Transparent", value));
        larvae::AssertEqual(value, int64_t{1});

        larvae::AssertFalse(info.ValueOf("Invalid", value));
        larvae::AssertFalse(info.ValueOf(nullptr, value));
    });

    auto test_enum_signed = larvae::RegisterTest("QueenReflectionPhase1", "EnumSignedValues", []() {
        const auto& info = queen::EnumInfo<Alignment>::Get();

        larvae::AssertEqual(info.underlying_size, sizeof(int32_t));

        int64_t value = 0;
        larvae::AssertTrue(info.ValueOf("Left", value));
        larvae::AssertEqual(value, int64_t{-1});

        larvae::AssertTrue(info.ValueOf("Right", value));
        larvae::AssertEqual(value, int64_t{1});
    });

    // ============================================================
    // Enum field detection tests
    // ============================================================

    auto test_enum_field_type = larvae::RegisterTest("QueenReflectionPhase1", "EnumFieldTypeDetection", []() {
        queen::ComponentReflector<> reflector;
        WithEnum::Reflect(reflector);

        larvae::AssertEqual(reflector.Count(), size_t{2});

        const auto& mode_field = reflector[0];
        larvae::AssertEqual(static_cast<int>(mode_field.type), static_cast<int>(queen::FieldType::Enum));
        larvae::AssertEqual(mode_field.size, sizeof(RenderMode));
        larvae::AssertNotNull(mode_field.enum_info);
        larvae::AssertEqual(mode_field.enum_info->entry_count, size_t{3});

        const auto& alpha_field = reflector[1];
        larvae::AssertEqual(static_cast<int>(alpha_field.type), static_cast<int>(queen::FieldType::Float32));
    });

    auto test_enum_unreflected_field = larvae::RegisterTest("QueenReflectionPhase1", "EnumUnreflectedNoEnumInfo", []() {
        queen::ComponentReflector<> reflector;
        WithUnreflectedEnum::Reflect(reflector);

        const auto& flag_field = reflector[0];
        larvae::AssertEqual(static_cast<int>(flag_field.type), static_cast<int>(queen::FieldType::Enum));
        larvae::AssertNull(flag_field.enum_info);
    });

    // ============================================================
    // Enum serialize/deserialize tests
    // ============================================================

    auto test_enum_serialize = larvae::RegisterTest("QueenReflectionPhase1", "EnumSerializeDeserialize", []() {
        WithEnum original{RenderMode::Wireframe, 0.5f};

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};
        queen::Serialize(original, writer);

        WithEnum loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(static_cast<int>(loaded.mode), static_cast<int>(RenderMode::Wireframe));
        larvae::AssertEqual(loaded.alpha, 0.5f);
    });

    auto test_enum_signed_serialize = larvae::RegisterTest("QueenReflectionPhase1", "EnumSignedSerializeDeserialize", []() {
        WithAlignment original{Alignment::Left, 42};

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};
        queen::Serialize(original, writer);

        WithAlignment loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(static_cast<int>(loaded.align), static_cast<int>(Alignment::Left));
        larvae::AssertEqual(loaded.padding, int32_t{42});
    });

    // ============================================================
    // FixedString field tests
    // ============================================================

    auto test_string_field_type = larvae::RegisterTest("QueenReflectionPhase1", "FixedStringFieldTypeDetection", []() {
        queen::ComponentReflector<> reflector;
        WithFixedString::Reflect(reflector);

        const auto& name_field = reflector[0];
        larvae::AssertEqual(static_cast<int>(name_field.type), static_cast<int>(queen::FieldType::String));
        larvae::AssertEqual(name_field.size, sizeof(wax::FixedString));
    });

    auto test_string_serialize = larvae::RegisterTest("QueenReflectionPhase1", "FixedStringSerializeDeserialize", []() {
        WithFixedString original{wax::FixedString{"Hello"}, 99};

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};
        queen::Serialize(original, writer);

        WithFixedString loaded{wax::FixedString{}, 0};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertTrue(loaded.name == wax::FixedString{"Hello"});
        larvae::AssertEqual(loaded.id, int32_t{99});
    });

    auto test_string_empty_serialize = larvae::RegisterTest("QueenReflectionPhase1", "FixedStringEmptySerialize", []() {
        WithFixedString original{wax::FixedString{}, 7};

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};
        queen::Serialize(original, writer);

        WithFixedString loaded{wax::FixedString{"garbage"}, 0};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertTrue(loaded.name.IsEmpty());
        larvae::AssertEqual(loaded.id, int32_t{7});
    });

    auto test_string_max_serialize = larvae::RegisterTest("QueenReflectionPhase1", "FixedStringMaxLenSerialize", []() {
        WithFixedString original{wax::FixedString{"1234567890123456789012"}, 1};  // 22 chars = max

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};
        queen::Serialize(original, writer);

        WithFixedString loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(loaded.name.Size(), size_t{22});
        larvae::AssertTrue(loaded.name == wax::FixedString{"1234567890123456789012"});
        larvae::AssertEqual(loaded.id, int32_t{1});
    });

    // ============================================================
    // FixedArray field tests
    // ============================================================

    auto test_array_field_type = larvae::RegisterTest("QueenReflectionPhase1", "FixedArrayFieldTypeDetection", []() {
        queen::ComponentReflector<> reflector;
        WithFixedArray::Reflect(reflector);

        const auto& values_field = reflector[0];
        larvae::AssertEqual(static_cast<int>(values_field.type), static_cast<int>(queen::FieldType::FixedArray));
        larvae::AssertEqual(values_field.element_count, size_t{4});
        larvae::AssertEqual(static_cast<int>(values_field.element_type), static_cast<int>(queen::FieldType::Float32));
        larvae::AssertEqual(values_field.size, sizeof(float) * 4);
    });

    auto test_array_serialize = larvae::RegisterTest("QueenReflectionPhase1", "FixedArraySerializeDeserialize", []() {
        WithFixedArray original{{1.f, 2.f, 3.f, 4.f}, 4};

        comb::LinearAllocator alloc{4096};
        wax::BinaryWriter<comb::LinearAllocator> writer{alloc};
        queen::Serialize(original, writer);

        WithFixedArray loaded{};
        wax::BinaryReader reader{writer.View()};
        queen::Deserialize(loaded, reader);

        larvae::AssertEqual(loaded.values[0], 1.f);
        larvae::AssertEqual(loaded.values[1], 2.f);
        larvae::AssertEqual(loaded.values[2], 3.f);
        larvae::AssertEqual(loaded.values[3], 4.f);
        larvae::AssertEqual(loaded.count, int32_t{4});
    });

    // ============================================================
    // FieldBuilder / FieldAttributes tests
    // ============================================================

    auto test_no_chaining_no_attributes = larvae::RegisterTest("QueenReflectionPhase1", "NoChainNoAttributes", []() {
        queen::ComponentReflector<> reflector;
        WithEnum::Reflect(reflector);

        // Fields without chaining should have nullptr attributes
        larvae::AssertNull(reflector[0].attributes);
        larvae::AssertNull(reflector[1].attributes);
    });

    auto test_range_annotation = larvae::RegisterTest("QueenReflectionPhase1", "RangeAnnotation", []() {
        queen::ComponentReflector<> reflector;
        WithAnnotations::Reflect(reflector);

        const auto& speed_field = reflector[0];
        larvae::AssertNotNull(speed_field.attributes);
        larvae::AssertTrue(speed_field.attributes->HasRange());
        larvae::AssertEqual(speed_field.attributes->min, 0.f);
        larvae::AssertEqual(speed_field.attributes->max, 100.f);
        larvae::AssertEqual(speed_field.attributes->step, 0.5f);
    });

    auto test_tooltip_annotation = larvae::RegisterTest("QueenReflectionPhase1", "TooltipAnnotation", []() {
        queen::ComponentReflector<> reflector;
        WithAnnotations::Reflect(reflector);

        const auto& speed_field = reflector[0];
        larvae::AssertNotNull(speed_field.attributes);
        larvae::AssertNotNull(speed_field.attributes->tooltip);
        larvae::AssertTrue(queen::detail::StringsEqual(
            speed_field.attributes->tooltip, "Movement speed in units/sec"));
    });

    auto test_category_annotation = larvae::RegisterTest("QueenReflectionPhase1", "CategoryAnnotation", []() {
        queen::ComponentReflector<> reflector;
        WithAnnotations::Reflect(reflector);

        const auto& speed_field = reflector[0];
        larvae::AssertNotNull(speed_field.attributes);
        larvae::AssertNotNull(speed_field.attributes->category);
        larvae::AssertTrue(queen::detail::StringsEqual(
            speed_field.attributes->category, "Movement"));
    });

    auto test_flag_annotation = larvae::RegisterTest("QueenReflectionPhase1", "FlagAnnotation", []() {
        queen::ComponentReflector<> reflector;
        WithAnnotations::Reflect(reflector);

        // health has ReadOnly flag
        const auto& health_field = reflector[1];
        larvae::AssertNotNull(health_field.attributes);
        larvae::AssertTrue(health_field.attributes->HasFlag(queen::FieldFlag::ReadOnly));
        larvae::AssertFalse(health_field.attributes->HasFlag(queen::FieldFlag::Hidden));

        // rotation has Angle flag
        const auto& rotation_field = reflector[2];
        larvae::AssertNotNull(rotation_field.attributes);
        larvae::AssertTrue(rotation_field.attributes->HasFlag(queen::FieldFlag::Angle));
    });

    auto test_display_name_annotation = larvae::RegisterTest("QueenReflectionPhase1", "DisplayNameAnnotation", []() {
        queen::ComponentReflector<> reflector;
        WithAnnotations::Reflect(reflector);

        const auto& rotation_field = reflector[2];
        larvae::AssertNotNull(rotation_field.attributes);
        larvae::AssertNotNull(rotation_field.attributes->display_name);
        larvae::AssertTrue(queen::detail::StringsEqual(
            rotation_field.attributes->display_name, "Rotation (deg)"));
    });

    auto test_unannotated_field_in_annotated_component = larvae::RegisterTest("QueenReflectionPhase1", "UnannotatedFieldNull", []() {
        queen::ComponentReflector<> reflector;
        WithAnnotations::Reflect(reflector);

        // mode field has no chaining — attributes should be nullptr
        const auto& mode_field = reflector[3];
        larvae::AssertNull(mode_field.attributes);
    });

    // ============================================================
    // Registry integration with new types
    // ============================================================

    auto test_registry_enum_component = larvae::RegisterTest("QueenReflectionPhase1", "RegistryEnumComponent", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<WithEnum>();

        const auto* found = registry.Find(queen::TypeIdOf<WithEnum>());
        larvae::AssertNotNull(found);
        larvae::AssertTrue(found->HasReflection());

        // Verify enum field is properly captured
        const auto* mode_field = found->reflection.FindField("mode");
        larvae::AssertNotNull(mode_field);
        larvae::AssertEqual(static_cast<int>(mode_field->type), static_cast<int>(queen::FieldType::Enum));
        larvae::AssertNotNull(mode_field->enum_info);
    });

    auto test_registry_string_component = larvae::RegisterTest("QueenReflectionPhase1", "RegistryStringComponent", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<WithFixedString>();

        const auto* found = registry.Find(queen::TypeIdOf<WithFixedString>());
        larvae::AssertNotNull(found);

        const auto* name_field = found->reflection.FindField("name");
        larvae::AssertNotNull(name_field);
        larvae::AssertEqual(static_cast<int>(name_field->type), static_cast<int>(queen::FieldType::String));
    });

    auto test_registry_array_component = larvae::RegisterTest("QueenReflectionPhase1", "RegistryArrayComponent", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<WithFixedArray>();

        const auto* found = registry.Find(queen::TypeIdOf<WithFixedArray>());
        larvae::AssertNotNull(found);

        const auto* values_field = found->reflection.FindField("values");
        larvae::AssertNotNull(values_field);
        larvae::AssertEqual(static_cast<int>(values_field->type), static_cast<int>(queen::FieldType::FixedArray));
        larvae::AssertEqual(values_field->element_count, size_t{4});
    });
}
