#include <larvae/larvae.h>
#include <nectar/hive/hive_writer.h>
#include <nectar/hive/hive_parser.h>
#include <comb/default_allocator.h>

namespace {

    auto& GetWriterAlloc()
    {
        static comb::ModuleAllocator alloc{"TestHiveWriter", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // Basic writing
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarHiveWriter", "EmptyDocument", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        auto text = nectar::HiveWriter::Write(doc, alloc);
        // Should at least have the header comment
        larvae::AssertTrue(text.View().Contains("Nectar"));
    });

    auto t2 = larvae::RegisterTest("NectarHiveWriter", "SingleSectionSingleValue", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        doc.SetValue("identity", "uuid", nectar::HiveValue::MakeString(alloc, "abc123"));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        larvae::AssertTrue(text.View().Contains("[identity]"));
        larvae::AssertTrue(text.View().Contains("uuid = \"abc123\""));
    });

    auto t3 = larvae::RegisterTest("NectarHiveWriter", "BoolValue", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        doc.SetValue("s", "flag", nectar::HiveValue::MakeBool(true));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        larvae::AssertTrue(text.View().Contains("flag = true"));
    });

    auto t4 = larvae::RegisterTest("NectarHiveWriter", "IntValue", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        doc.SetValue("s", "count", nectar::HiveValue::MakeInt(42));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        larvae::AssertTrue(text.View().Contains("count = 42"));
    });

    auto t5 = larvae::RegisterTest("NectarHiveWriter", "FloatValue", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        doc.SetValue("s", "ratio", nectar::HiveValue::MakeFloat(0.5));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        larvae::AssertTrue(text.View().Contains("ratio = 0.5"));
    });

    auto t6 = larvae::RegisterTest("NectarHiveWriter", "StringArrayValue", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        auto arr = nectar::HiveValue::MakeStringArray(alloc);
        arr.PushString(alloc, "alpha");
        arr.PushString(alloc, "beta");
        doc.SetValue("s", "tags", static_cast<nectar::HiveValue&&>(arr));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        larvae::AssertTrue(text.View().Contains("[\"alpha\", \"beta\"]"));
    });

    auto t7 = larvae::RegisterTest("NectarHiveWriter", "EmptyStringArray", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        doc.SetValue("s", "list", nectar::HiveValue::MakeStringArray(alloc));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        larvae::AssertTrue(text.View().Contains("list = []"));
    });

    // =========================================================================
    // Section ordering
    // =========================================================================

    auto t8 = larvae::RegisterTest("NectarHiveWriter", "SectionsSortedAlphabetically", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        doc.SetValue("tags", "a", nectar::HiveValue::MakeString(alloc, "x"));
        doc.SetValue("identity", "b", nectar::HiveValue::MakeString(alloc, "y"));
        doc.SetValue("import", "c", nectar::HiveValue::MakeString(alloc, "z"));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        auto view = text.View();

        // identity should come before import, import before tags
        auto pos_id = view.Find("[identity]");
        auto pos_im = view.Find("[import]");
        auto pos_ta = view.Find("[tags]");
        larvae::AssertTrue(pos_id != wax::StringView::npos);
        larvae::AssertTrue(pos_im != wax::StringView::npos);
        larvae::AssertTrue(pos_ta != wax::StringView::npos);
        larvae::AssertTrue(pos_id < pos_im);
        larvae::AssertTrue(pos_im < pos_ta);
    });

    // =========================================================================
    // Escape handling
    // =========================================================================

    auto t9 = larvae::RegisterTest("NectarHiveWriter", "EscapesQuotesInStrings", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        doc.SetValue("s", "val", nectar::HiveValue::MakeString(alloc, "say \"hi\""));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        larvae::AssertTrue(text.View().Contains("say \\\"hi\\\""));
    });

    // =========================================================================
    // Round-trip consistency
    // =========================================================================

    auto t10 = larvae::RegisterTest("NectarHiveWriter", "RoundTripPreservesAllTypes", []() {
        auto& alloc = GetWriterAlloc();
        nectar::HiveDocument doc{alloc};
        doc.SetValue("s", "name", nectar::HiveValue::MakeString(alloc, "test"));
        doc.SetValue("s", "flag", nectar::HiveValue::MakeBool(false));
        doc.SetValue("s", "num", nectar::HiveValue::MakeInt(-7));
        doc.SetValue("s", "flt", nectar::HiveValue::MakeFloat(3.14));
        auto arr = nectar::HiveValue::MakeStringArray(alloc);
        arr.PushString(alloc, "x");
        doc.SetValue("s", "arr", static_cast<nectar::HiveValue&&>(arr));

        auto text = nectar::HiveWriter::Write(doc, alloc);
        auto result = nectar::HiveParser::Parse(text.View(), alloc);
        larvae::AssertTrue(result.Success());

        larvae::AssertTrue(result.document.GetString("s", "name").Equals("test"));
        larvae::AssertEqual(result.document.GetBool("s", "flag"), false);
        larvae::AssertEqual(result.document.GetInt("s", "num"), int64_t{-7});
        larvae::AssertDoubleEqual(result.document.GetFloat("s", "flt"), 3.14);

        auto* a = result.document.GetValue("s", "arr");
        larvae::AssertNotNull(a);
        larvae::AssertEqual(a->AsStringArray().Size(), size_t{1});
        larvae::AssertTrue(a->AsStringArray()[0].View().Equals("x"));
    });

}
