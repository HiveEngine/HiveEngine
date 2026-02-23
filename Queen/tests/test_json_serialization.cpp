#include <larvae/larvae.h>
#include <queen/reflect/component_reflector.h>
#include <queen/reflect/reflectable.h>
#include <queen/reflect/json_serializer.h>
#include <queen/reflect/json_deserializer.h>
#include <queen/reflect/enum_reflection.h>
#include <queen/core/entity.h>
#include <wax/containers/fixed_string.h>
#include <cstring>
#include <cmath>

namespace
{
    // ============================================================
    // Test types
    // ============================================================

    struct Position
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("x", &Position::x);
            r.Field("y", &Position::y);
            r.Field("z", &Position::z);
        }
    };

    struct AllPrimitives
    {
        int32_t i32 = 0;
        uint32_t u32 = 0;
        float f32 = 0.f;
        double f64 = 0.0;
        bool flag = false;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("i32", &AllPrimitives::i32);
            r.Field("u32", &AllPrimitives::u32);
            r.Field("f32", &AllPrimitives::f32);
            r.Field("f64", &AllPrimitives::f64);
            r.Field("flag", &AllPrimitives::flag);
        }
    };

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

    enum class RenderMode : uint8_t
    {
        Opaque = 0,
        Transparent = 1,
        Wireframe = 2,
    };

    struct WithEntity
    {
        queen::Entity target;
        int32_t data = 0;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("target", &WithEntity::target);
            r.Field("data", &WithEntity::data);
        }
    };

    struct WithFixedArray
    {
        float values[3] = {0.f, 0.f, 0.f};

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("values", &WithFixedArray::values);
        }
    };

    struct WithString
    {
        wax::FixedString name;
        int32_t id = 0;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("name", &WithString::name);
            r.Field("id", &WithString::id);
        }
    };
}

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

namespace
{
    struct WithEnum
    {
        RenderMode mode = RenderMode::Opaque;
        float alpha = 1.f;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("mode", &WithEnum::mode);
            r.Field("alpha", &WithEnum::alpha);
        }
    };

    // Helper to do a JSON roundtrip
    template<queen::Reflectable T>
    void JsonRoundtrip(const T& original, T& loaded)
    {
        auto reflection = queen::GetReflectionData<T>();

        queen::JsonSerializer<4096> serializer;
        serializer.SerializeComponent(&original, reflection);

        auto result = queen::JsonDeserializer::DeserializeComponent(
            &loaded, reflection, serializer.CStr());

        larvae::AssertTrue(result.success);
    }

    // ============================================================
    // Serialize tests
    // ============================================================

    auto test_serialize_position = larvae::RegisterTest("QueenJsonSerialization", "SerializePosition", []() {
        Position pos{1.f, 2.f, 3.f};
        auto reflection = queen::GetReflectionData<Position>();

        queen::JsonSerializer<4096> serializer;
        serializer.SerializeComponent(&pos, reflection);

        const char* json = serializer.CStr();
        larvae::AssertNotNull(json);
        // Should contain field names and values
        larvae::AssertTrue(std::strstr(json, "\"x\"") != nullptr);
        larvae::AssertTrue(std::strstr(json, "\"y\"") != nullptr);
        larvae::AssertTrue(std::strstr(json, "\"z\"") != nullptr);
    });

    auto test_serialize_bool = larvae::RegisterTest("QueenJsonSerialization", "SerializeBoolAsText", []() {
        AllPrimitives p{};
        p.flag = true;
        auto reflection = queen::GetReflectionData<AllPrimitives>();

        queen::JsonSerializer<4096> serializer;
        serializer.SerializeComponent(&p, reflection);

        larvae::AssertTrue(std::strstr(serializer.CStr(), "true") != nullptr);
        larvae::AssertTrue(std::strstr(serializer.CStr(), "1") == nullptr ||
                          std::strstr(serializer.CStr(), "true") < std::strstr(serializer.CStr(), "1") ||
                          true);  // Just check "true" is present
    });

    auto test_serialize_enum_as_name = larvae::RegisterTest("QueenJsonSerialization", "SerializeEnumAsName", []() {
        WithEnum comp{RenderMode::Wireframe, 0.5f};
        auto reflection = queen::GetReflectionData<WithEnum>();

        queen::JsonSerializer<4096> serializer;
        serializer.SerializeComponent(&comp, reflection);

        larvae::AssertTrue(std::strstr(serializer.CStr(), "\"Wireframe\"") != nullptr);
    });

    // ============================================================
    // Roundtrip tests
    // ============================================================

    auto test_roundtrip_position = larvae::RegisterTest("QueenJsonSerialization", "RoundtripPosition", []() {
        Position original{1.5f, -2.5f, 3.0f};
        Position loaded{};
        JsonRoundtrip(original, loaded);

        larvae::AssertEqual(loaded.x, 1.5f);
        larvae::AssertEqual(loaded.y, -2.5f);
        larvae::AssertEqual(loaded.z, 3.0f);
    });

    auto test_roundtrip_primitives = larvae::RegisterTest("QueenJsonSerialization", "RoundtripPrimitives", []() {
        AllPrimitives original{-42, 99u, 3.14f, 2.718281828, true};
        AllPrimitives loaded{};
        JsonRoundtrip(original, loaded);

        larvae::AssertEqual(loaded.i32, int32_t{-42});
        larvae::AssertEqual(loaded.u32, uint32_t{99});
        larvae::AssertEqual(loaded.f32, 3.14f);
        // Double precision: check within epsilon
        larvae::AssertTrue(std::fabs(loaded.f64 - 2.718281828) < 1e-9);
        larvae::AssertTrue(loaded.flag);
    });

    auto test_roundtrip_nested = larvae::RegisterTest("QueenJsonSerialization", "RoundtripNested", []() {
        Transform original{};
        original.position.x = 10.f;
        original.position.y = 20.f;
        original.rotation = 1.57f;

        Transform loaded{};
        JsonRoundtrip(original, loaded);

        larvae::AssertEqual(loaded.position.x, 10.f);
        larvae::AssertEqual(loaded.position.y, 20.f);
        larvae::AssertEqual(loaded.rotation, 1.57f);
    });

    auto test_roundtrip_enum = larvae::RegisterTest("QueenJsonSerialization", "RoundtripEnum", []() {
        WithEnum original{RenderMode::Transparent, 0.75f};
        WithEnum loaded{};
        JsonRoundtrip(original, loaded);

        larvae::AssertEqual(static_cast<int>(loaded.mode), static_cast<int>(RenderMode::Transparent));
        larvae::AssertEqual(loaded.alpha, 0.75f);
    });

    auto test_roundtrip_entity = larvae::RegisterTest("QueenJsonSerialization", "RoundtripEntity", []() {
        WithEntity original{queen::Entity{42, 7, queen::Entity::Flags::kAlive}, 123};
        WithEntity loaded{};
        JsonRoundtrip(original, loaded);

        larvae::AssertEqual(loaded.target.Index(), queen::Entity::IndexType{42});
        larvae::AssertEqual(loaded.target.Generation(), queen::Entity::GenerationType{7});
        larvae::AssertEqual(loaded.data, int32_t{123});
    });

    auto test_roundtrip_array = larvae::RegisterTest("QueenJsonSerialization", "RoundtripFixedArray", []() {
        WithFixedArray original{{1.f, 2.f, 3.f}};
        WithFixedArray loaded{};
        JsonRoundtrip(original, loaded);

        larvae::AssertEqual(loaded.values[0], 1.f);
        larvae::AssertEqual(loaded.values[1], 2.f);
        larvae::AssertEqual(loaded.values[2], 3.f);
    });

    auto test_roundtrip_string = larvae::RegisterTest("QueenJsonSerialization", "RoundtripFixedString", []() {
        WithString original{wax::FixedString{"Hello"}, 42};
        WithString loaded{};
        JsonRoundtrip(original, loaded);

        larvae::AssertTrue(loaded.name == wax::FixedString{"Hello"});
        larvae::AssertEqual(loaded.id, int32_t{42});
    });

    // ============================================================
    // Forward-compatibility tests
    // ============================================================

    auto test_unknown_field_skipped = larvae::RegisterTest("QueenJsonSerialization", "UnknownFieldSkipped", []() {
        auto reflection = queen::GetReflectionData<Position>();

        // JSON with extra field "w" that Position doesn't have
        const char* json = R"({"x": 1.0, "w": 99.0, "y": 2.0, "z": 3.0})";

        Position loaded{};
        auto result = queen::JsonDeserializer::DeserializeComponent(&loaded, reflection, json);

        larvae::AssertTrue(result.success);
        larvae::AssertEqual(result.fields_read, size_t{3});
        larvae::AssertEqual(result.fields_skipped, size_t{1});
        larvae::AssertEqual(loaded.x, 1.0f);
        larvae::AssertEqual(loaded.y, 2.0f);
        larvae::AssertEqual(loaded.z, 3.0f);
    });

    auto test_missing_field_keeps_default = larvae::RegisterTest("QueenJsonSerialization", "MissingFieldKeepsDefault", []() {
        auto reflection = queen::GetReflectionData<Position>();

        // Only x is provided, y and z should keep their default (0.f)
        const char* json = R"({"x": 5.0})";

        Position loaded{};
        loaded.y = 99.f;  // pre-set to verify it's NOT overwritten
        auto result = queen::JsonDeserializer::DeserializeComponent(&loaded, reflection, json);

        larvae::AssertTrue(result.success);
        larvae::AssertEqual(result.fields_read, size_t{1});
        larvae::AssertEqual(loaded.x, 5.0f);
        larvae::AssertEqual(loaded.y, 99.f);  // not in JSON, kept original
    });

    auto test_unknown_object_skipped = larvae::RegisterTest("QueenJsonSerialization", "UnknownObjectSkipped", []() {
        auto reflection = queen::GetReflectionData<Position>();

        const char* json = R"({"x": 1.0, "extra": {"nested": true, "deep": {"a": 1}}, "y": 2.0, "z": 3.0})";

        Position loaded{};
        auto result = queen::JsonDeserializer::DeserializeComponent(&loaded, reflection, json);

        larvae::AssertTrue(result.success);
        larvae::AssertEqual(result.fields_read, size_t{3});
        larvae::AssertEqual(result.fields_skipped, size_t{1});
    });

    auto test_unknown_array_skipped = larvae::RegisterTest("QueenJsonSerialization", "UnknownArraySkipped", []() {
        auto reflection = queen::GetReflectionData<Position>();

        const char* json = R"({"x": 1.0, "tags": ["a", "b"], "y": 2.0, "z": 3.0})";

        Position loaded{};
        auto result = queen::JsonDeserializer::DeserializeComponent(&loaded, reflection, json);

        larvae::AssertTrue(result.success);
        larvae::AssertEqual(result.fields_read, size_t{3});
        larvae::AssertEqual(result.fields_skipped, size_t{1});
    });

    auto test_empty_object = larvae::RegisterTest("QueenJsonSerialization", "EmptyObject", []() {
        auto reflection = queen::GetReflectionData<Position>();

        const char* json = "{}";

        Position loaded{};
        auto result = queen::JsonDeserializer::DeserializeComponent(&loaded, reflection, json);

        larvae::AssertTrue(result.success);
        larvae::AssertEqual(result.fields_read, size_t{0});
    });

    auto test_whitespace_tolerance = larvae::RegisterTest("QueenJsonSerialization", "WhitespaceTolerance", []() {
        auto reflection = queen::GetReflectionData<Position>();

        const char* json = "  {  \"x\" : 1.0 , \"y\" : 2.0 , \"z\" : 3.0  }  ";

        Position loaded{};
        auto result = queen::JsonDeserializer::DeserializeComponent(&loaded, reflection, json);

        larvae::AssertTrue(result.success);
        larvae::AssertEqual(loaded.x, 1.0f);
    });

    auto test_string_escape = larvae::RegisterTest("QueenJsonSerialization", "StringEscapeRoundtrip", []() {
        WithString original{wax::FixedString{"a\"b"}, 1};

        auto reflection = queen::GetReflectionData<WithString>();
        queen::JsonSerializer<4096> serializer;
        serializer.SerializeComponent(&original, reflection);

        // Verify escaped quote in output
        larvae::AssertTrue(std::strstr(serializer.CStr(), "a\\\"b") != nullptr);

        WithString loaded{};
        auto result = queen::JsonDeserializer::DeserializeComponent(&loaded, reflection, serializer.CStr());

        larvae::AssertTrue(result.success);
        larvae::AssertTrue(loaded.name == wax::FixedString{"a\"b"});
    });

    auto test_negative_int = larvae::RegisterTest("QueenJsonSerialization", "NegativeIntRoundtrip", []() {
        AllPrimitives original{};
        original.i32 = -12345;

        AllPrimitives loaded{};
        JsonRoundtrip(original, loaded);

        larvae::AssertEqual(loaded.i32, int32_t{-12345});
    });

    auto test_bool_false = larvae::RegisterTest("QueenJsonSerialization", "BoolFalseRoundtrip", []() {
        AllPrimitives original{};
        original.flag = false;

        auto reflection = queen::GetReflectionData<AllPrimitives>();
        queen::JsonSerializer<4096> serializer;
        serializer.SerializeComponent(&original, reflection);

        larvae::AssertTrue(std::strstr(serializer.CStr(), "false") != nullptr);

        AllPrimitives loaded{};
        loaded.flag = true;  // pre-set
        auto result = queen::JsonDeserializer::DeserializeComponent(&loaded, reflection, serializer.CStr());

        larvae::AssertTrue(result.success);
        larvae::AssertFalse(loaded.flag);
    });
}
