#include <larvae/larvae.h>
#include <wax/containers/fixed_string.h>
#include <wax/containers/string_view.h>

namespace {
    // =============================================================================
    // Construction
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxFixedString", "DefaultConstructor", []() {
        wax::FixedString str;

        larvae::AssertEqual(str.Size(), 0u);
        larvae::AssertTrue(str.IsEmpty());
        larvae::AssertEqual(str.Capacity(), wax::FixedString::MaxCapacity);
    });

    auto test2 = larvae::RegisterTest("WaxFixedString", "CStringConstructor", []() {
        wax::FixedString str{"Hello"};

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertFalse(str.IsEmpty());
        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[4], 'o');
    });

    auto test3 = larvae::RegisterTest("WaxFixedString", "CStringConstructorMaxCapacity", []() {
        wax::FixedString str{"1234567890123456789012"};

        larvae::AssertEqual(str.Size(), 22u);
        larvae::AssertTrue(str.IsFull());
    });

    auto test4 = larvae::RegisterTest("WaxFixedString", "StringViewConstructor", []() {
        wax::StringView sv{"World"};
        wax::FixedString str{sv};

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertEqual(str[0], 'W');
        larvae::AssertEqual(str[4], 'd');
    });

    auto test5 = larvae::RegisterTest("WaxFixedString", "PointerSizeConstructor", []() {
        const char* data = "Test";
        wax::FixedString str{data, 4};

        larvae::AssertEqual(str.Size(), 4u);
        larvae::AssertEqual(str[0], 'T');
        larvae::AssertEqual(str[3], 't');
    });

    auto test6 = larvae::RegisterTest("WaxFixedString", "EmptyStringConstructor", []() {
        wax::FixedString str{""};

        larvae::AssertEqual(str.Size(), 0u);
        larvae::AssertTrue(str.IsEmpty());
    });

    auto test7 = larvae::RegisterTest("WaxFixedString", "NullptrConstructor", []() {
        wax::FixedString str{nullptr};

        larvae::AssertEqual(str.Size(), 0u);
        larvae::AssertTrue(str.IsEmpty());
    });

    // =============================================================================
    // Copy and Move
    // =============================================================================

    auto test8 = larvae::RegisterTest("WaxFixedString", "CopyConstructor", []() {
        wax::FixedString str1{"Hello"};
        wax::FixedString str2{str1};

        larvae::AssertEqual(str2.Size(), 5u);
        larvae::AssertEqual(str2[0], 'H');
        larvae::AssertEqual(str2[4], 'o');
    });

    auto test9 = larvae::RegisterTest("WaxFixedString", "CopyAssignment", []() {
        wax::FixedString str1{"Hello"};
        wax::FixedString str2{"World"};

        str2 = str1;

        larvae::AssertEqual(str2.Size(), 5u);
        larvae::AssertEqual(str2[0], 'H');
        larvae::AssertEqual(str2[4], 'o');
    });

    auto test10 = larvae::RegisterTest("WaxFixedString", "MoveConstructor", []() {
        wax::FixedString str1{"Hello"};
        wax::FixedString str2{std::move(str1)};

        larvae::AssertEqual(str2.Size(), 5u);
        larvae::AssertEqual(str2[0], 'H');
        larvae::AssertEqual(str2[4], 'o');
    });

    auto test11 = larvae::RegisterTest("WaxFixedString", "MoveAssignment", []() {
        wax::FixedString str1{"Hello"};
        wax::FixedString str2{"World"};

        str2 = std::move(str1);

        larvae::AssertEqual(str2.Size(), 5u);
        larvae::AssertEqual(str2[0], 'H');
        larvae::AssertEqual(str2[4], 'o');
    });

    // =============================================================================
    // Element Access
    // =============================================================================

    auto test12 = larvae::RegisterTest("WaxFixedString", "IndexOperator", []() {
        wax::FixedString str{"Hello"};

        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[1], 'e');
        larvae::AssertEqual(str[2], 'l');
        larvae::AssertEqual(str[3], 'l');
        larvae::AssertEqual(str[4], 'o');
    });

    auto test13 = larvae::RegisterTest("WaxFixedString", "IndexOperatorWrite", []() {
        wax::FixedString str{"Hello"};

        str[0] = 'Y';
        str[4] = 'a';

        larvae::AssertEqual(str[0], 'Y');
        larvae::AssertEqual(str[4], 'a');
    });

    auto test14 = larvae::RegisterTest("WaxFixedString", "AtMethod", []() {
        wax::FixedString str{"Test"};

        larvae::AssertEqual(str.At(0), 'T');
        larvae::AssertEqual(str.At(3), 't');
    });

    auto test15 = larvae::RegisterTest("WaxFixedString", "FrontBack", []() {
        wax::FixedString str{"Hello"};

        larvae::AssertEqual(str.Front(), 'H');
        larvae::AssertEqual(str.Back(), 'o');
    });

    auto test16 = larvae::RegisterTest("WaxFixedString", "CStrNullTerminated", []() {
        wax::FixedString str{"Hello"};

        const char* c_str = str.CStr();
        larvae::AssertEqual(c_str[0], 'H');
        larvae::AssertEqual(c_str[5], '\0');
    });

    // =============================================================================
    // Iterators
    // =============================================================================

    auto test17 = larvae::RegisterTest("WaxFixedString", "RangeBasedFor", []() {
        wax::FixedString str{"abc"};

        char result[3];
        size_t i = 0;
        for (char ch : str) {
            result[i++] = ch;
        }

        larvae::AssertEqual(result[0], 'a');
        larvae::AssertEqual(result[1], 'b');
        larvae::AssertEqual(result[2], 'c');
    });

    // =============================================================================
    // StringView Conversion
    // =============================================================================

    auto test18 = larvae::RegisterTest("WaxFixedString", "ViewConversion", []() {
        wax::FixedString str{"Hello"};

        wax::StringView sv = str.View();

        larvae::AssertEqual(sv.Size(), 5u);
        larvae::AssertEqual(sv[0], 'H');
        larvae::AssertEqual(sv[4], 'o');
    });

    auto test19 = larvae::RegisterTest("WaxFixedString", "ImplicitStringViewConversion", []() {
        wax::FixedString str{"World"};

        wax::StringView sv = str;

        larvae::AssertEqual(sv.Size(), 5u);
        larvae::AssertEqual(sv[0], 'W');
    });

    // =============================================================================
    // Modifiers
    // =============================================================================

    auto test20 = larvae::RegisterTest("WaxFixedString", "Clear", []() {
        wax::FixedString str{"Hello"};

        str.Clear();

        larvae::AssertEqual(str.Size(), 0u);
        larvae::AssertTrue(str.IsEmpty());
    });

    auto test21 = larvae::RegisterTest("WaxFixedString", "AppendChar", []() {
        wax::FixedString str{"Hello"};

        str.Append('!');

        larvae::AssertEqual(str.Size(), 6u);
        larvae::AssertEqual(str[5], '!');
    });

    auto test22 = larvae::RegisterTest("WaxFixedString", "AppendCString", []() {
        wax::FixedString str{"Hello"};

        str.Append(" World");

        larvae::AssertEqual(str.Size(), 11u);
        larvae::AssertEqual(str[5], ' ');
        larvae::AssertEqual(str[6], 'W');
    });

    auto test23 = larvae::RegisterTest("WaxFixedString", "AppendStringView", []() {
        wax::FixedString str{"Hello"};
        wax::StringView sv{" there"};

        str.Append(sv);

        larvae::AssertEqual(str.Size(), 11u);
        larvae::AssertEqual(str[5], ' ');
        larvae::AssertEqual(str[6], 't');
    });

    auto test24 = larvae::RegisterTest("WaxFixedString", "AppendPointerAndCount", []() {
        wax::FixedString str{"Hello"};
        const char* data = " World!!!";

        str.Append(data, 6);

        larvae::AssertEqual(str.Size(), 11u);
        larvae::AssertEqual(str[5], ' ');
        larvae::AssertEqual(str[10], 'd');
    });

    auto test25 = larvae::RegisterTest("WaxFixedString", "AppendToCapacity", []() {
        wax::FixedString str{"Hello"};

        str.Append(" World12345678");  // 5 + 14 = 19

        larvae::AssertEqual(str.Size(), 19u);
        larvae::AssertFalse(str.IsFull());
    });

    auto test26 = larvae::RegisterTest("WaxFixedString", "PopBack", []() {
        wax::FixedString str{"Hello"};

        str.PopBack();

        larvae::AssertEqual(str.Size(), 4u);
        larvae::AssertEqual(str[3], 'l');
    });

    auto test27 = larvae::RegisterTest("WaxFixedString", "ResizeGrow", []() {
        wax::FixedString str{"Hi"};

        str.Resize(5, 'x');

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[1], 'i');
        larvae::AssertEqual(str[2], 'x');
        larvae::AssertEqual(str[3], 'x');
        larvae::AssertEqual(str[4], 'x');
    });

    auto test28 = larvae::RegisterTest("WaxFixedString", "ResizeShrink", []() {
        wax::FixedString str{"Hello World"};

        str.Resize(5);

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[4], 'o');
    });

    // =============================================================================
    // Search Operations
    // =============================================================================

    auto test29 = larvae::RegisterTest("WaxFixedString", "FindChar", []() {
        wax::FixedString str{"Hello World"};

        larvae::AssertEqual(str.Find('o'), 4u);
        larvae::AssertEqual(str.Find('W'), 6u);
        larvae::AssertEqual(str.Find('x'), wax::FixedString::npos);
    });

    auto test30 = larvae::RegisterTest("WaxFixedString", "FindSubstring", []() {
        wax::FixedString str{"Hello World"};

        larvae::AssertEqual(str.Find("World"), 6u);
        larvae::AssertEqual(str.Find("xyz"), wax::FixedString::npos);
    });

    auto test31 = larvae::RegisterTest("WaxFixedString", "Contains", []() {
        wax::FixedString str{"Hello World"};

        larvae::AssertTrue(str.Contains('H'));
        larvae::AssertTrue(str.Contains("World"));
        larvae::AssertFalse(str.Contains('x'));
        larvae::AssertFalse(str.Contains("xyz"));
    });

    auto test32 = larvae::RegisterTest("WaxFixedString", "StartsWith", []() {
        wax::FixedString str{"Hello World"};

        larvae::AssertTrue(str.StartsWith('H'));
        larvae::AssertTrue(str.StartsWith("Hello"));
        larvae::AssertFalse(str.StartsWith('W'));
        larvae::AssertFalse(str.StartsWith("World"));
    });

    auto test33 = larvae::RegisterTest("WaxFixedString", "EndsWith", []() {
        wax::FixedString str{"Hello World"};

        larvae::AssertTrue(str.EndsWith('d'));
        larvae::AssertTrue(str.EndsWith("World"));
        larvae::AssertFalse(str.EndsWith('H'));
        larvae::AssertFalse(str.EndsWith("Hello"));
    });

    // =============================================================================
    // Comparison Operations
    // =============================================================================

    auto test34 = larvae::RegisterTest("WaxFixedString", "CompareEqual", []() {
        wax::FixedString str1{"Hello"};
        wax::FixedString str2{"Hello"};

        larvae::AssertEqual(str1.Compare(str2), 0);
        larvae::AssertTrue(str1.Equals(str2));
    });

    auto test35 = larvae::RegisterTest("WaxFixedString", "CompareLess", []() {
        wax::FixedString str1{"Apple"};
        wax::FixedString str2{"Banana"};

        larvae::AssertTrue(str1.Compare(str2) < 0);
    });

    auto test36 = larvae::RegisterTest("WaxFixedString", "EqualityOperators", []() {
        wax::FixedString str1{"Hello"};
        wax::FixedString str2{"Hello"};
        wax::FixedString str3{"World"};

        larvae::AssertTrue(str1 == str2);
        larvae::AssertFalse(str1 == str3);
        larvae::AssertTrue(str1 != str3);
    });

    auto test37 = larvae::RegisterTest("WaxFixedString", "ComparisonOperators", []() {
        wax::FixedString str1{"Apple"};
        wax::FixedString str2{"Banana"};

        larvae::AssertTrue(str1 < str2);
        larvae::AssertTrue(str1 <= str2);
        larvae::AssertTrue(str2 > str1);
        larvae::AssertTrue(str2 >= str1);
    });

    auto test38 = larvae::RegisterTest("WaxFixedString", "CompareWithStringView", []() {
        wax::FixedString str{"Hello"};
        wax::StringView sv{"Hello"};

        larvae::AssertTrue(str == sv);
        larvae::AssertTrue(sv == str);
    });

    auto test39 = larvae::RegisterTest("WaxFixedString", "CompareWithCString", []() {
        wax::FixedString str{"Hello"};

        larvae::AssertTrue(str == "Hello");
        larvae::AssertTrue("Hello" == str);
        larvae::AssertFalse(str == "World");
    });

    // =============================================================================
    // Edge Cases - Capacity Limits
    // =============================================================================

    auto test40 = larvae::RegisterTest("WaxFixedString", "MaxCapacity22Chars", []() {
        wax::FixedString str{"1234567890123456789012"};

        larvae::AssertEqual(str.Size(), 22u);
        larvae::AssertTrue(str.IsFull());
        larvae::AssertEqual(str.Capacity(), wax::FixedString::MaxCapacity);
    });

    auto test41 = larvae::RegisterTest("WaxFixedString", "AppendAtCapacity", []() {
        wax::FixedString str{"1234567890123456789012"};

        larvae::AssertTrue(str.IsFull());

        // Should not append beyond capacity
        str.Append('X');

        larvae::AssertEqual(str.Size(), 22u);
    });

    auto test42 = larvae::RegisterTest("WaxFixedString", "ConstructorTruncatesLongString", []() {
        wax::FixedString str{"This is a very long string that exceeds capacity"};

        // Should truncate to 22 chars
        larvae::AssertEqual(str.Size(), 22u);
        larvae::AssertTrue(str.IsFull());
    });

    auto test43 = larvae::RegisterTest("WaxFixedString", "ResizeBeyondCapacity", []() {
        wax::FixedString str{"Hello"};

        str.Resize(30, 'x');  // Should clamp to MaxCapacity

        larvae::AssertEqual(str.Size(), wax::FixedString::MaxCapacity);
        larvae::AssertTrue(str.IsFull());
    });

    auto test44 = larvae::RegisterTest("WaxFixedString", "AppendTruncates", []() {
        wax::FixedString str{"Hello World"};

        str.Append("12345678901234567890");  // Would exceed capacity

        // Should stop at MaxCapacity
        larvae::AssertEqual(str.Size(), wax::FixedString::MaxCapacity);
        larvae::AssertTrue(str.IsFull());
    });

    // =============================================================================
    // Constexpr Usage
    // =============================================================================

    auto test45 = larvae::RegisterTest("WaxFixedString", "ConstexprConstructor", []() {
        constexpr wax::FixedString str{"Hello"};

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertEqual(str[0], 'H');
    });

    auto test46 = larvae::RegisterTest("WaxFixedString", "ConstexprOperations", []() {
        constexpr wax::FixedString str{"Test"};

        constexpr char first = str.Front();
        constexpr char last = str.Back();
        constexpr size_t size = str.Size();

        larvae::AssertEqual(first, 'T');
        larvae::AssertEqual(last, 't');
        larvae::AssertEqual(size, 4u);
    });

    // =============================================================================
    // Empty String
    // =============================================================================

    auto test47 = larvae::RegisterTest("WaxFixedString", "EmptyStringOperations", []() {
        wax::FixedString str{};

        larvae::AssertTrue(str.IsEmpty());
        larvae::AssertFalse(str.IsFull());
        larvae::AssertEqual(str.Size(), 0u);
        larvae::AssertEqual(str.Find('x'), wax::FixedString::npos);
        larvae::AssertFalse(str.Contains('x'));
    });

    auto test48 = larvae::RegisterTest("WaxFixedString", "SingleCharString", []() {
        wax::FixedString str{"A"};

        larvae::AssertEqual(str.Size(), 1u);
        larvae::AssertEqual(str.Front(), 'A');
        larvae::AssertEqual(str.Back(), 'A');
        larvae::AssertTrue(str.StartsWith('A'));
        larvae::AssertTrue(str.EndsWith('A'));
    });
}
