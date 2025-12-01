#include <larvae/larvae.h>
#include <wax/containers/string_view.h>

namespace {
    // =============================================================================
    // Construction
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxStringView", "DefaultConstructor", []() {
        wax::StringView sv;

        larvae::AssertEqual(sv.Size(), 0u);
        larvae::AssertTrue(sv.IsEmpty());
        larvae::AssertNull(sv.Data());
    });

    auto test2 = larvae::RegisterTest("WaxStringView", "PointerSizeConstructor", []() {
        const char* str = "Hello";
        wax::StringView sv{str, 5};

        larvae::AssertEqual(sv.Size(), 5u);
        larvae::AssertFalse(sv.IsEmpty());
        larvae::AssertEqual(sv[0], 'H');
        larvae::AssertEqual(sv[4], 'o');
    });

    auto test3 = larvae::RegisterTest("WaxStringView", "CStringConstructor", []() {
        const char* str = "World";
        wax::StringView sv{str};

        larvae::AssertEqual(sv.Size(), 5u);
        larvae::AssertEqual(sv[0], 'W');
        larvae::AssertEqual(sv[4], 'd');
    });

    auto test4 = larvae::RegisterTest("WaxStringView", "StringLiteralConstructor", []() {
        wax::StringView sv{"Test"};

        larvae::AssertEqual(sv.Size(), 4u);
        larvae::AssertEqual(sv[0], 'T');
        larvae::AssertEqual(sv[3], 't');
    });

    auto test5 = larvae::RegisterTest("WaxStringView", "EmptyStringConstructor", []() {
        wax::StringView sv{""};

        larvae::AssertEqual(sv.Size(), 0u);
        larvae::AssertTrue(sv.IsEmpty());
    });

    auto test6 = larvae::RegisterTest("WaxStringView", "NullptrConstructor", []() {
        wax::StringView sv{nullptr};

        larvae::AssertEqual(sv.Size(), 0u);
        larvae::AssertTrue(sv.IsEmpty());
    });

    // =============================================================================
    // Element Access
    // =============================================================================

    auto test7 = larvae::RegisterTest("WaxStringView", "IndexOperator", []() {
        wax::StringView sv{"Hello"};

        larvae::AssertEqual(sv[0], 'H');
        larvae::AssertEqual(sv[1], 'e');
        larvae::AssertEqual(sv[2], 'l');
        larvae::AssertEqual(sv[3], 'l');
        larvae::AssertEqual(sv[4], 'o');
    });

    auto test8 = larvae::RegisterTest("WaxStringView", "AtMethod", []() {
        wax::StringView sv{"Test"};

        larvae::AssertEqual(sv.At(0), 'T');
        larvae::AssertEqual(sv.At(1), 'e');
        larvae::AssertEqual(sv.At(2), 's');
        larvae::AssertEqual(sv.At(3), 't');
    });

    auto test9 = larvae::RegisterTest("WaxStringView", "FrontBack", []() {
        wax::StringView sv{"Hello"};

        larvae::AssertEqual(sv.Front(), 'H');
        larvae::AssertEqual(sv.Back(), 'o');
    });

    // =============================================================================
    // Iterators
    // =============================================================================

    auto test10 = larvae::RegisterTest("WaxStringView", "RangeBasedFor", []() {
        wax::StringView sv{"abc"};

        char result[3];
        size_t i = 0;
        for (char ch : sv) {
            result[i++] = ch;
        }

        larvae::AssertEqual(result[0], 'a');
        larvae::AssertEqual(result[1], 'b');
        larvae::AssertEqual(result[2], 'c');
    });

    auto test11 = larvae::RegisterTest("WaxStringView", "IteratorAccess", []() {
        wax::StringView sv{"Test"};

        auto it = sv.begin();
        larvae::AssertEqual(*it, 'T');
        ++it;
        larvae::AssertEqual(*it, 'e');
        ++it;
        larvae::AssertEqual(*it, 's');
        ++it;
        larvae::AssertEqual(*it, 't');
        ++it;
        larvae::AssertTrue(it == sv.end());
    });

    // =============================================================================
    // Substring Operations
    // =============================================================================

    auto test12 = larvae::RegisterTest("WaxStringView", "SubstrFromStart", []() {
        wax::StringView sv{"Hello World"};
        wax::StringView sub = sv.Substr(0, 5);

        larvae::AssertEqual(sub.Size(), 5u);
        larvae::AssertEqual(sub[0], 'H');
        larvae::AssertEqual(sub[4], 'o');
    });

    auto test13 = larvae::RegisterTest("WaxStringView", "SubstrMiddle", []() {
        wax::StringView sv{"Hello World"};
        wax::StringView sub = sv.Substr(6, 5);

        larvae::AssertEqual(sub.Size(), 5u);
        larvae::AssertEqual(sub[0], 'W');
        larvae::AssertEqual(sub[4], 'd');
    });

    auto test14 = larvae::RegisterTest("WaxStringView", "SubstrToEnd", []() {
        wax::StringView sv{"Hello World"};
        wax::StringView sub = sv.Substr(6);

        larvae::AssertEqual(sub.Size(), 5u);
        larvae::AssertEqual(sub[0], 'W');
        larvae::AssertEqual(sub[4], 'd');
    });

    auto test15 = larvae::RegisterTest("WaxStringView", "RemovePrefix", []() {
        wax::StringView sv{"Hello World"};
        wax::StringView result = sv.RemovePrefix(6);

        larvae::AssertEqual(result.Size(), 5u);
        larvae::AssertEqual(result[0], 'W');
    });

    auto test16 = larvae::RegisterTest("WaxStringView", "RemoveSuffix", []() {
        wax::StringView sv{"Hello World"};
        wax::StringView result = sv.RemoveSuffix(6);

        larvae::AssertEqual(result.Size(), 5u);
        larvae::AssertEqual(result[0], 'H');
        larvae::AssertEqual(result[4], 'o');
    });

    // =============================================================================
    // Search Operations - Find
    // =============================================================================

    auto test17 = larvae::RegisterTest("WaxStringView", "FindCharFound", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertEqual(sv.Find('H'), 0u);
        larvae::AssertEqual(sv.Find('o'), 4u);
        larvae::AssertEqual(sv.Find('W'), 6u);
    });

    auto test18 = larvae::RegisterTest("WaxStringView", "FindCharNotFound", []() {
        wax::StringView sv{"Hello"};

        larvae::AssertEqual(sv.Find('x'), wax::StringView::npos);
        larvae::AssertEqual(sv.Find('z'), wax::StringView::npos);
    });

    auto test19 = larvae::RegisterTest("WaxStringView", "FindCharWithPosition", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertEqual(sv.Find('o', 0), 4u);
        larvae::AssertEqual(sv.Find('o', 5), 7u);
        larvae::AssertEqual(sv.Find('l', 3), 3u);
    });

    auto test20 = larvae::RegisterTest("WaxStringView", "FindSubstringFound", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertEqual(sv.Find("Hello"), 0u);
        larvae::AssertEqual(sv.Find("World"), 6u);
        larvae::AssertEqual(sv.Find("lo"), 3u);
    });

    auto test21 = larvae::RegisterTest("WaxStringView", "FindSubstringNotFound", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertEqual(sv.Find("xyz"), wax::StringView::npos);
        larvae::AssertEqual(sv.Find("Test"), wax::StringView::npos);
    });

    auto test22 = larvae::RegisterTest("WaxStringView", "FindEmptySubstring", []() {
        wax::StringView sv{"Hello"};

        larvae::AssertEqual(sv.Find(""), 0u);
        larvae::AssertEqual(sv.Find("", 3), 3u);
    });

    auto test23 = larvae::RegisterTest("WaxStringView", "RFindChar", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertEqual(sv.RFind('o'), 7u);
        larvae::AssertEqual(sv.RFind('l'), 9u);
        larvae::AssertEqual(sv.RFind('H'), 0u);
    });

    auto test24 = larvae::RegisterTest("WaxStringView", "RFindCharNotFound", []() {
        wax::StringView sv{"Hello"};

        larvae::AssertEqual(sv.RFind('x'), wax::StringView::npos);
    });

    // =============================================================================
    // Search Operations - Contains
    // =============================================================================

    auto test25 = larvae::RegisterTest("WaxStringView", "ContainsChar", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertTrue(sv.Contains('H'));
        larvae::AssertTrue(sv.Contains('o'));
        larvae::AssertTrue(sv.Contains(' '));
        larvae::AssertFalse(sv.Contains('x'));
        larvae::AssertFalse(sv.Contains('z'));
    });

    auto test26 = larvae::RegisterTest("WaxStringView", "ContainsSubstring", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertTrue(sv.Contains("Hello"));
        larvae::AssertTrue(sv.Contains("World"));
        larvae::AssertTrue(sv.Contains("lo Wo"));
        larvae::AssertFalse(sv.Contains("xyz"));
        larvae::AssertFalse(sv.Contains("Test"));
    });

    // =============================================================================
    // Search Operations - StartsWith / EndsWith
    // =============================================================================

    auto test27 = larvae::RegisterTest("WaxStringView", "StartsWithChar", []() {
        wax::StringView sv{"Hello"};

        larvae::AssertTrue(sv.StartsWith('H'));
        larvae::AssertFalse(sv.StartsWith('e'));
        larvae::AssertFalse(sv.StartsWith('o'));
    });

    auto test28 = larvae::RegisterTest("WaxStringView", "StartsWithSubstring", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertTrue(sv.StartsWith("Hello"));
        larvae::AssertTrue(sv.StartsWith("Hel"));
        larvae::AssertTrue(sv.StartsWith("H"));
        larvae::AssertFalse(sv.StartsWith("World"));
        larvae::AssertFalse(sv.StartsWith("ello"));
    });

    auto test29 = larvae::RegisterTest("WaxStringView", "EndsWithChar", []() {
        wax::StringView sv{"Hello"};

        larvae::AssertTrue(sv.EndsWith('o'));
        larvae::AssertFalse(sv.EndsWith('l'));
        larvae::AssertFalse(sv.EndsWith('H'));
    });

    auto test30 = larvae::RegisterTest("WaxStringView", "EndsWithSubstring", []() {
        wax::StringView sv{"Hello World"};

        larvae::AssertTrue(sv.EndsWith("World"));
        larvae::AssertTrue(sv.EndsWith("orld"));
        larvae::AssertTrue(sv.EndsWith("d"));
        larvae::AssertFalse(sv.EndsWith("Hello"));
        larvae::AssertFalse(sv.EndsWith("Worl"));
    });

    // =============================================================================
    // Comparison Operations
    // =============================================================================

    auto test31 = larvae::RegisterTest("WaxStringView", "CompareEqual", []() {
        wax::StringView sv1{"Hello"};
        wax::StringView sv2{"Hello"};

        larvae::AssertEqual(sv1.Compare(sv2), 0);
    });

    auto test32 = larvae::RegisterTest("WaxStringView", "CompareLess", []() {
        wax::StringView sv1{"Apple"};
        wax::StringView sv2{"Banana"};

        larvae::AssertTrue(sv1.Compare(sv2) < 0);
    });

    auto test33 = larvae::RegisterTest("WaxStringView", "CompareGreater", []() {
        wax::StringView sv1{"Zebra"};
        wax::StringView sv2{"Apple"};

        larvae::AssertTrue(sv1.Compare(sv2) > 0);
    });

    auto test34 = larvae::RegisterTest("WaxStringView", "CompareDifferentLengths", []() {
        wax::StringView sv1{"Hello"};
        wax::StringView sv2{"Hello World"};

        larvae::AssertTrue(sv1.Compare(sv2) < 0);
        larvae::AssertTrue(sv2.Compare(sv1) > 0);
    });

    auto test35 = larvae::RegisterTest("WaxStringView", "Equals", []() {
        wax::StringView sv1{"Hello"};
        wax::StringView sv2{"Hello"};
        wax::StringView sv3{"World"};

        larvae::AssertTrue(sv1.Equals(sv2));
        larvae::AssertFalse(sv1.Equals(sv3));
    });

    auto test36 = larvae::RegisterTest("WaxStringView", "EqualityOperator", []() {
        wax::StringView sv1{"Hello"};
        wax::StringView sv2{"Hello"};
        wax::StringView sv3{"World"};

        larvae::AssertTrue(sv1 == sv2);
        larvae::AssertFalse(sv1 == sv3);
    });

    auto test37 = larvae::RegisterTest("WaxStringView", "InequalityOperator", []() {
        wax::StringView sv1{"Hello"};
        wax::StringView sv2{"World"};

        larvae::AssertTrue(sv1 != sv2);
        larvae::AssertFalse(sv1 != sv1);
    });

    auto test38 = larvae::RegisterTest("WaxStringView", "ComparisonOperators", []() {
        wax::StringView sv1{"Apple"};
        wax::StringView sv2{"Banana"};

        larvae::AssertTrue(sv1 < sv2);
        larvae::AssertTrue(sv1 <= sv2);
        larvae::AssertTrue(sv2 > sv1);
        larvae::AssertTrue(sv2 >= sv1);
    });

    // =============================================================================
    // Edge Cases
    // =============================================================================

    auto test39 = larvae::RegisterTest("WaxStringView", "EmptyStringOperations", []() {
        wax::StringView sv{""};

        larvae::AssertTrue(sv.IsEmpty());
        larvae::AssertEqual(sv.Size(), 0u);
        larvae::AssertEqual(sv.Find('x'), wax::StringView::npos);
        larvae::AssertFalse(sv.Contains('x'));
        larvae::AssertTrue(sv.StartsWith(""));
        larvae::AssertTrue(sv.EndsWith(""));
    });

    auto test40 = larvae::RegisterTest("WaxStringView", "SingleCharString", []() {
        wax::StringView sv{"A"};

        larvae::AssertEqual(sv.Size(), 1u);
        larvae::AssertEqual(sv.Front(), 'A');
        larvae::AssertEqual(sv.Back(), 'A');
        larvae::AssertTrue(sv.StartsWith('A'));
        larvae::AssertTrue(sv.EndsWith('A'));
    });
}
