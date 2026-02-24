#include <larvae/larvae.h>
#include <nectar/hive/hive_parser.h>
#include <nectar/hive/hive_writer.h>
#include <comb/default_allocator.h>

namespace {

    auto& GetParserAlloc()
    {
        static comb::ModuleAllocator alloc{"TestHiveParser", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // Empty / minimal
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarHiveParser", "EmptyDocument", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertEqual(result.document.Sections().Count(), size_t{0});
    });

    auto t2 = larvae::RegisterTest("NectarHiveParser", "CommentsOnly", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("# comment\n# another\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertEqual(result.document.Sections().Count(), size_t{0});
    });

    auto t3 = larvae::RegisterTest("NectarHiveParser", "EmptySection", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[empty]\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertTrue(result.document.HasSection("empty"));
    });

    auto t4 = larvae::RegisterTest("NectarHiveParser", "BlankLinesIgnored", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("\n\n[sec]\n\nkey = \"val\"\n\n", alloc);
        larvae::AssertTrue(result.Success());
        auto sv = result.document.GetString("sec", "key");
        larvae::AssertTrue(sv.Equals("val"));
    });

    // =========================================================================
    // Value types
    // =========================================================================

    auto t5 = larvae::RegisterTest("NectarHiveParser", "StringValue", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nname = \"hello world\"\n", alloc);
        larvae::AssertTrue(result.Success());
        auto* v = result.document.GetValue("s", "name");
        larvae::AssertNotNull(v);
        larvae::AssertTrue(v->type == nectar::HiveValue::Type::String);
        larvae::AssertTrue(v->AsString().Equals("hello world"));
    });

    auto t6 = larvae::RegisterTest("NectarHiveParser", "BoolValues", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\na = true\nb = false\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertEqual(result.document.GetBool("s", "a"), true);
        larvae::AssertEqual(result.document.GetBool("s", "b"), false);
    });

    auto t7 = larvae::RegisterTest("NectarHiveParser", "IntValue", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\ncount = 42\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertEqual(result.document.GetInt("s", "count"), int64_t{42});
    });

    auto t8 = larvae::RegisterTest("NectarHiveParser", "NegativeInt", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nval = -100\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertEqual(result.document.GetInt("s", "val"), int64_t{-100});
    });

    auto t9 = larvae::RegisterTest("NectarHiveParser", "FloatValue", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nratio = 0.5\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertDoubleEqual(result.document.GetFloat("s", "ratio"), 0.5);
    });

    auto t10 = larvae::RegisterTest("NectarHiveParser", "FloatScientific", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nval = 1.5e3\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertDoubleEqual(result.document.GetFloat("s", "val"), 1500.0);
    });

    auto t11 = larvae::RegisterTest("NectarHiveParser", "StringArray", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\ntags = [\"a\", \"b\", \"c\"]\n", alloc);
        larvae::AssertTrue(result.Success());
        auto* v = result.document.GetValue("s", "tags");
        larvae::AssertNotNull(v);
        larvae::AssertTrue(v->type == nectar::HiveValue::Type::StringArray);
        larvae::AssertEqual(v->AsStringArray().Size(), size_t{3});
        larvae::AssertTrue(v->AsStringArray()[0].View().Equals("a"));
        larvae::AssertTrue(v->AsStringArray()[1].View().Equals("b"));
        larvae::AssertTrue(v->AsStringArray()[2].View().Equals("c"));
    });

    auto t12 = larvae::RegisterTest("NectarHiveParser", "EmptyStringArray", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\ntags = []\n", alloc);
        larvae::AssertTrue(result.Success());
        auto* v = result.document.GetValue("s", "tags");
        larvae::AssertNotNull(v);
        larvae::AssertTrue(v->type == nectar::HiveValue::Type::StringArray);
        larvae::AssertEqual(v->AsStringArray().Size(), size_t{0});
    });

    // =========================================================================
    // Sections
    // =========================================================================

    auto t13 = larvae::RegisterTest("NectarHiveParser", "MultipleSections", []() {
        auto& alloc = GetParserAlloc();
        const char* input =
            "[identity]\n"
            "uuid = \"abc123\"\n"
            "[import]\n"
            "format = \"BC7\"\n";
        auto result = nectar::HiveParser::Parse(input, alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertTrue(result.document.HasSection("identity"));
        larvae::AssertTrue(result.document.HasSection("import"));
        larvae::AssertTrue(result.document.GetString("identity", "uuid").Equals("abc123"));
        larvae::AssertTrue(result.document.GetString("import", "format").Equals("BC7"));
    });

    auto t14 = larvae::RegisterTest("NectarHiveParser", "DottedSection", []() {
        auto& alloc = GetParserAlloc();
        const char* input =
            "[import]\n"
            "format = \"BC7\"\n"
            "[import.platform.mobile]\n"
            "format = \"ASTC4x4\"\n";
        auto result = nectar::HiveParser::Parse(input, alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertTrue(result.document.HasSection("import.platform.mobile"));
        larvae::AssertTrue(result.document.GetString("import", "format").Equals("BC7"));
        larvae::AssertTrue(result.document.GetString("import.platform.mobile", "format").Equals("ASTC4x4"));
    });

    // =========================================================================
    // String escapes
    // =========================================================================

    auto t15 = larvae::RegisterTest("NectarHiveParser", "EscapedQuotes", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nval = \"say \\\"hi\\\"\"\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertTrue(result.document.GetString("s", "val").Equals("say \"hi\""));
    });

    auto t16 = larvae::RegisterTest("NectarHiveParser", "EscapedNewline", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nval = \"line1\\nline2\"\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertTrue(result.document.GetString("s", "val").Equals("line1\nline2"));
    });

    // =========================================================================
    // Whitespace handling
    // =========================================================================

    auto t17 = larvae::RegisterTest("NectarHiveParser", "WhitespaceAroundEquals", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\n  key  =  \"val\"  \n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertTrue(result.document.GetString("s", "key").Equals("val"));
    });

    auto t18 = larvae::RegisterTest("NectarHiveParser", "TabsAndSpaces", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\n\tkey\t=\t42\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertEqual(result.document.GetInt("s", "key"), int64_t{42});
    });

    // =========================================================================
    // Error cases
    // =========================================================================

    auto t19 = larvae::RegisterTest("NectarHiveParser", "ErrorEmptySectionName", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[]\nkey = \"val\"\n", alloc);
        larvae::AssertFalse(result.Success());
        larvae::AssertTrue(result.errors.Size() > 0);
    });

    auto t20 = larvae::RegisterTest("NectarHiveParser", "ErrorValueBeforeSection", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("key = \"val\"\n", alloc);
        larvae::AssertFalse(result.Success());
    });

    auto t21 = larvae::RegisterTest("NectarHiveParser", "ErrorMissingEquals", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nthis is not valid\n", alloc);
        larvae::AssertFalse(result.Success());
    });

    auto t22 = larvae::RegisterTest("NectarHiveParser", "ErrorUnterminatedString", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nval = \"unterminated\n", alloc);
        larvae::AssertFalse(result.Success());
    });

    auto t23 = larvae::RegisterTest("NectarHiveParser", "ErrorUnterminatedArray", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\nval = [\"a\", \"b\"\n", alloc);
        larvae::AssertFalse(result.Success());
    });

    // =========================================================================
    // Convenience getters with fallback
    // =========================================================================

    auto t24 = larvae::RegisterTest("NectarHiveParser", "FallbackOnMissingKey", []() {
        auto& alloc = GetParserAlloc();
        auto result = nectar::HiveParser::Parse("[s]\n", alloc);
        larvae::AssertTrue(result.Success());
        larvae::AssertEqual(result.document.GetInt("s", "missing", 999), int64_t{999});
        larvae::AssertEqual(result.document.GetBool("s", "missing", true), true);
        larvae::AssertDoubleEqual(result.document.GetFloat("s", "missing", 1.5), 1.5);
    });

    // =========================================================================
    // Full .hive example
    // =========================================================================

    auto t25 = larvae::RegisterTest("NectarHiveParser", "FullTextureHive", []() {
        auto& alloc = GetParserAlloc();
        const char* input =
            "# hero.png.hive\n"
            "\n"
            "[identity]\n"
            "uuid = \"a3b5c7d9e1f2a3b5c7d9e1f2a3b5c7d9\"\n"
            "type = \"Texture\"\n"
            "\n"
            "[import]\n"
            "format = \"BC7\"\n"
            "generate_mipmaps = true\n"
            "max_size = 2048\n"
            "srgb = true\n"
            "\n"
            "[import.platform.mobile]\n"
            "format = \"ASTC4x4\"\n"
            "max_size = 1024\n"
            "\n"
            "[tags]\n"
            "labels = [\"character\", \"hero\"]\n"
            "group = \"core\"\n"
            "\n"
            "[source]\n"
            "content_hash = \"7f3a8b1c2d3e\"\n"
            "import_version = 3\n";

        auto result = nectar::HiveParser::Parse(input, alloc);
        larvae::AssertTrue(result.Success());

        larvae::AssertTrue(result.document.GetString("identity", "uuid")
            .Equals("a3b5c7d9e1f2a3b5c7d9e1f2a3b5c7d9"));
        larvae::AssertTrue(result.document.GetString("identity", "type").Equals("Texture"));
        larvae::AssertTrue(result.document.GetString("import", "format").Equals("BC7"));
        larvae::AssertEqual(result.document.GetBool("import", "generate_mipmaps"), true);
        larvae::AssertEqual(result.document.GetInt("import", "max_size"), int64_t{2048});
        larvae::AssertTrue(result.document.GetString("import.platform.mobile", "format").Equals("ASTC4x4"));
        larvae::AssertEqual(result.document.GetInt("import.platform.mobile", "max_size"), int64_t{1024});

        auto* labels = result.document.GetValue("tags", "labels");
        larvae::AssertNotNull(labels);
        larvae::AssertEqual(labels->AsStringArray().Size(), size_t{2});
        larvae::AssertTrue(labels->AsStringArray()[0].View().Equals("character"));
        larvae::AssertTrue(labels->AsStringArray()[1].View().Equals("hero"));

        larvae::AssertEqual(result.document.GetInt("source", "import_version"), int64_t{3});
    });

    // =========================================================================
    // Round-trip
    // =========================================================================

    auto t26 = larvae::RegisterTest("NectarHiveParser", "RoundTrip", []() {
        auto& alloc = GetParserAlloc();
        const char* input =
            "[identity]\n"
            "type = \"Mesh\"\n"
            "uuid = \"abc\"\n"
            "[import]\n"
            "scale = 0.01\n"
            "optimize = true\n";

        auto result1 = nectar::HiveParser::Parse(input, alloc);
        larvae::AssertTrue(result1.Success());

        auto written = nectar::HiveWriter::Write(result1.document, alloc);
        auto result2 = nectar::HiveParser::Parse(written.View(), alloc);
        larvae::AssertTrue(result2.Success());

        larvae::AssertTrue(result2.document.GetString("identity", "uuid").Equals("abc"));
        larvae::AssertTrue(result2.document.GetString("identity", "type").Equals("Mesh"));
        larvae::AssertEqual(result2.document.GetBool("import", "optimize"), true);
        larvae::AssertDoubleEqual(result2.document.GetFloat("import", "scale"), 0.01);
    });

}
