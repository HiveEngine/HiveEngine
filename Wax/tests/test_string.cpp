#include <larvae/larvae.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/linear_allocator.h>

namespace {
    // =============================================================================
    // Construction
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxString", "DefaultConstructor", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc};

        larvae::AssertEqual(str.Size(), 0u);
        larvae::AssertTrue(str.IsEmpty());
        larvae::AssertEqual(str.Capacity(), wax::String<comb::LinearAllocator>::SsoCapacity);
    });

    auto test2 = larvae::RegisterTest("WaxString", "CStringConstructorSSO", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertFalse(str.IsEmpty());
        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[4], 'o');
        larvae::AssertEqual(str.Capacity(), wax::String<comb::LinearAllocator>::SsoCapacity);
    });

    auto test3 = larvae::RegisterTest("WaxString", "CStringConstructorHeap", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "This is a very long string that exceeds SSO capacity"};

        larvae::AssertEqual(str.Size(), 52u);
        larvae::AssertFalse(str.IsEmpty());
        larvae::AssertTrue(str.Capacity() > wax::String<comb::LinearAllocator>::SsoCapacity);
    });

    auto test4 = larvae::RegisterTest("WaxString", "StringViewConstructor", []() {
        comb::LinearAllocator alloc{1024};
        wax::StringView sv{"World"};
        wax::String<comb::LinearAllocator> str{alloc, sv};

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertEqual(str[0], 'W');
        larvae::AssertEqual(str[4], 'd');
    });

    auto test5 = larvae::RegisterTest("WaxString", "PointerSizeConstructor", []() {
        comb::LinearAllocator alloc{1024};
        const char* data = "Test";
        wax::String<comb::LinearAllocator> str{alloc, data, 4};

        larvae::AssertEqual(str.Size(), 4u);
        larvae::AssertEqual(str[0], 'T');
        larvae::AssertEqual(str[3], 't');
    });

    auto test6 = larvae::RegisterTest("WaxString", "EmptyStringConstructor", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, ""};

        larvae::AssertEqual(str.Size(), 0u);
        larvae::AssertTrue(str.IsEmpty());
    });

    // =============================================================================
    // Copy and Move
    // =============================================================================

    auto test7 = larvae::RegisterTest("WaxString", "CopyConstructorSSO", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Hello"};
        wax::String<comb::LinearAllocator> str2{str1};

        larvae::AssertEqual(str2.Size(), 5u);
        larvae::AssertEqual(str2[0], 'H');
        larvae::AssertEqual(str2[4], 'o');
    });

    auto test8 = larvae::RegisterTest("WaxString", "CopyConstructorHeap", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "This is a very long string that exceeds SSO capacity"};
        wax::String<comb::LinearAllocator> str2{str1};

        larvae::AssertEqual(str2.Size(), str1.Size());
        larvae::AssertEqual(str2[0], 'T');
    });

    auto test9 = larvae::RegisterTest("WaxString", "CopyAssignment", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Hello"};
        wax::String<comb::LinearAllocator> str2{alloc, "World"};

        str2 = str1;

        larvae::AssertEqual(str2.Size(), 5u);
        larvae::AssertEqual(str2[0], 'H');
        larvae::AssertEqual(str2[4], 'o');
    });

    auto test10 = larvae::RegisterTest("WaxString", "MoveConstructor", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Hello"};
        wax::String<comb::LinearAllocator> str2{std::move(str1)};

        larvae::AssertEqual(str2.Size(), 5u);
        larvae::AssertEqual(str2[0], 'H');
        larvae::AssertEqual(str2[4], 'o');
    });

    auto test11 = larvae::RegisterTest("WaxString", "MoveAssignment", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Hello"};
        wax::String<comb::LinearAllocator> str2{alloc, "World"};

        str2 = std::move(str1);

        larvae::AssertEqual(str2.Size(), 5u);
        larvae::AssertEqual(str2[0], 'H');
        larvae::AssertEqual(str2[4], 'o');
    });

    // =============================================================================
    // Element Access
    // =============================================================================

    auto test12 = larvae::RegisterTest("WaxString", "IndexOperator", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[1], 'e');
        larvae::AssertEqual(str[2], 'l');
        larvae::AssertEqual(str[3], 'l');
        larvae::AssertEqual(str[4], 'o');
    });

    auto test13 = larvae::RegisterTest("WaxString", "IndexOperatorWrite", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        str[0] = 'Y';
        str[4] = 'a';

        larvae::AssertEqual(str[0], 'Y');
        larvae::AssertEqual(str[4], 'a');
    });

    auto test14 = larvae::RegisterTest("WaxString", "AtMethod", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Test"};

        larvae::AssertEqual(str.At(0), 'T');
        larvae::AssertEqual(str.At(3), 't');
    });

    auto test15 = larvae::RegisterTest("WaxString", "FrontBack", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        larvae::AssertEqual(str.Front(), 'H');
        larvae::AssertEqual(str.Back(), 'o');
    });

    auto test16 = larvae::RegisterTest("WaxString", "CStrNullTerminated", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        const char* c_str = str.CStr();
        larvae::AssertEqual(c_str[0], 'H');
        larvae::AssertEqual(c_str[5], '\0');
    });

    // =============================================================================
    // Iterators
    // =============================================================================

    auto test17 = larvae::RegisterTest("WaxString", "RangeBasedFor", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "abc"};

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

    auto test18 = larvae::RegisterTest("WaxString", "ViewConversion", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        wax::StringView sv = str.View();

        larvae::AssertEqual(sv.Size(), 5u);
        larvae::AssertEqual(sv[0], 'H');
        larvae::AssertEqual(sv[4], 'o');
    });

    auto test19 = larvae::RegisterTest("WaxString", "ImplicitStringViewConversion", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "World"};

        wax::StringView sv = str;

        larvae::AssertEqual(sv.Size(), 5u);
        larvae::AssertEqual(sv[0], 'W');
    });

    // =============================================================================
    // Capacity Management
    // =============================================================================

    auto test20 = larvae::RegisterTest("WaxString", "ReserveSSO", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hi"};

        str.Reserve(10);

        larvae::AssertEqual(str.Size(), 2u);
        larvae::AssertTrue(str.Capacity() >= 10u);
        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[1], 'i');
    });

    auto test21 = larvae::RegisterTest("WaxString", "ReserveHeap", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "This is a very long string that exceeds SSO capacity"};
        size_t old_capacity = str.Capacity();

        str.Reserve(old_capacity + 100);

        larvae::AssertTrue(str.Capacity() >= old_capacity + 100);
    });

    auto test22 = larvae::RegisterTest("WaxString", "ShrinkToFitHeapToSSO", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "This is a very long string"};
        str.Resize(5);

        str.ShrinkToFit();

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertEqual(str.Capacity(), wax::String<comb::LinearAllocator>::SsoCapacity);
    });

    // =============================================================================
    // Modifiers
    // =============================================================================

    auto test23 = larvae::RegisterTest("WaxString", "Clear", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        str.Clear();

        larvae::AssertEqual(str.Size(), 0u);
        larvae::AssertTrue(str.IsEmpty());
    });

    auto test24 = larvae::RegisterTest("WaxString", "AppendChar", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        str.Append('!');

        larvae::AssertEqual(str.Size(), 6u);
        larvae::AssertEqual(str[5], '!');
    });

    auto test25 = larvae::RegisterTest("WaxString", "AppendCString", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        str.Append(" World");

        larvae::AssertEqual(str.Size(), 11u);
        larvae::AssertEqual(str[5], ' ');
        larvae::AssertEqual(str[6], 'W');
    });

    auto test26 = larvae::RegisterTest("WaxString", "AppendStringView", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};
        wax::StringView sv{" there"};

        str.Append(sv);

        larvae::AssertEqual(str.Size(), 11u);
        larvae::AssertEqual(str[5], ' ');
        larvae::AssertEqual(str[6], 't');
    });

    auto test27 = larvae::RegisterTest("WaxString", "AppendPointerAndCount", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};
        const char* data = " World!!!";

        str.Append(data, 6);

        larvae::AssertEqual(str.Size(), 11u);
        larvae::AssertEqual(str[5], ' ');
        larvae::AssertEqual(str[10], 'd');
    });

    auto test28 = larvae::RegisterTest("WaxString", "AppendMultipleSSO", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hi"};

        str.Append('!');
        str.Append("!!");
        str.Append("!!!");

        larvae::AssertEqual(str.Size(), 8u);
        larvae::AssertEqual(str.Capacity(), wax::String<comb::LinearAllocator>::SsoCapacity);
    });

    auto test29 = larvae::RegisterTest("WaxString", "AppendSSOToHeap", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Short"};

        str.Append(" string that will exceed SSO capacity for sure");

        larvae::AssertTrue(str.Size() > wax::String<comb::LinearAllocator>::SsoCapacity);
        larvae::AssertTrue(str.Capacity() > wax::String<comb::LinearAllocator>::SsoCapacity);
    });

    auto test30 = larvae::RegisterTest("WaxString", "PopBack", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        str.PopBack();

        larvae::AssertEqual(str.Size(), 4u);
        larvae::AssertEqual(str[3], 'l');
    });

    auto test31 = larvae::RegisterTest("WaxString", "ResizeGrow", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hi"};

        str.Resize(5, 'x');

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[1], 'i');
        larvae::AssertEqual(str[2], 'x');
        larvae::AssertEqual(str[3], 'x');
        larvae::AssertEqual(str[4], 'x');
    });

    auto test32 = larvae::RegisterTest("WaxString", "ResizeShrink", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello World"};

        str.Resize(5);

        larvae::AssertEqual(str.Size(), 5u);
        larvae::AssertEqual(str[0], 'H');
        larvae::AssertEqual(str[4], 'o');
    });

    // =============================================================================
    // Search Operations
    // =============================================================================

    auto test33 = larvae::RegisterTest("WaxString", "FindChar", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello World"};

        larvae::AssertEqual(str.Find('o'), 4u);
        larvae::AssertEqual(str.Find('W'), 6u);
        larvae::AssertEqual(str.Find('x'), wax::String<comb::LinearAllocator>::npos);
    });

    auto test34 = larvae::RegisterTest("WaxString", "FindSubstring", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello World"};

        larvae::AssertEqual(str.Find("World"), 6u);
        larvae::AssertEqual(str.Find("xyz"), wax::String<comb::LinearAllocator>::npos);
    });

    auto test35 = larvae::RegisterTest("WaxString", "Contains", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello World"};

        larvae::AssertTrue(str.Contains('H'));
        larvae::AssertTrue(str.Contains("World"));
        larvae::AssertFalse(str.Contains('x'));
        larvae::AssertFalse(str.Contains("xyz"));
    });

    auto test36 = larvae::RegisterTest("WaxString", "StartsWith", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello World"};

        larvae::AssertTrue(str.StartsWith('H'));
        larvae::AssertTrue(str.StartsWith("Hello"));
        larvae::AssertFalse(str.StartsWith('W'));
        larvae::AssertFalse(str.StartsWith("World"));
    });

    auto test37 = larvae::RegisterTest("WaxString", "EndsWith", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello World"};

        larvae::AssertTrue(str.EndsWith('d'));
        larvae::AssertTrue(str.EndsWith("World"));
        larvae::AssertFalse(str.EndsWith('H'));
        larvae::AssertFalse(str.EndsWith("Hello"));
    });

    // =============================================================================
    // Comparison Operations
    // =============================================================================

    auto test38 = larvae::RegisterTest("WaxString", "CompareEqual", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Hello"};
        wax::String<comb::LinearAllocator> str2{alloc, "Hello"};

        larvae::AssertEqual(str1.Compare(str2), 0);
        larvae::AssertTrue(str1.Equals(str2));
    });

    auto test39 = larvae::RegisterTest("WaxString", "CompareLess", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Apple"};
        wax::String<comb::LinearAllocator> str2{alloc, "Banana"};

        larvae::AssertTrue(str1.Compare(str2) < 0);
    });

    auto test40 = larvae::RegisterTest("WaxString", "EqualityOperators", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Hello"};
        wax::String<comb::LinearAllocator> str2{alloc, "Hello"};
        wax::String<comb::LinearAllocator> str3{alloc, "World"};

        larvae::AssertTrue(str1 == str2);
        larvae::AssertFalse(str1 == str3);
        larvae::AssertTrue(str1 != str3);
    });

    auto test41 = larvae::RegisterTest("WaxString", "ComparisonOperators", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Apple"};
        wax::String<comb::LinearAllocator> str2{alloc, "Banana"};

        larvae::AssertTrue(str1 < str2);
        larvae::AssertTrue(str1 <= str2);
        larvae::AssertTrue(str2 > str1);
        larvae::AssertTrue(str2 >= str1);
    });

    auto test42 = larvae::RegisterTest("WaxString", "CompareWithStringView", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};
        wax::StringView sv{"Hello"};

        larvae::AssertTrue(str == sv);
        larvae::AssertTrue(sv == str);
    });

    // =============================================================================
    // Concatenation
    // =============================================================================

    auto test43 = larvae::RegisterTest("WaxString", "ConcatenateStrings", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Hello"};
        wax::String<comb::LinearAllocator> str2{alloc, " World"};

        wax::String<comb::LinearAllocator> result = str1 + str2;

        larvae::AssertEqual(result.Size(), 11u);
        larvae::AssertTrue(result == "Hello World");
    });

    auto test44 = larvae::RegisterTest("WaxString", "ConcatenateStringAndCString", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "Hello"};

        wax::String<comb::LinearAllocator> result = str + " World";

        larvae::AssertEqual(result.Size(), 11u);
        larvae::AssertTrue(result == "Hello World");
    });

    // =============================================================================
    // SSO Edge Cases
    // =============================================================================

    auto test45 = larvae::RegisterTest("WaxString", "SSOBoundary22Chars", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "1234567890123456789012"};

        larvae::AssertEqual(str.Size(), 22u);
        larvae::AssertEqual(str.Capacity(), wax::String<comb::LinearAllocator>::SsoCapacity);
    });

    auto test46 = larvae::RegisterTest("WaxString", "SSOBoundary23Chars", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "12345678901234567890123"};

        larvae::AssertEqual(str.Size(), 23u);
        larvae::AssertTrue(str.Capacity() > wax::String<comb::LinearAllocator>::SsoCapacity);
    });

    auto test47 = larvae::RegisterTest("WaxString", "AppendAcrossSSOBoundary", []() {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "1234567890123456789012"};

        larvae::AssertEqual(str.Capacity(), wax::String<comb::LinearAllocator>::SsoCapacity);

        str.Append('X');

        larvae::AssertEqual(str.Size(), 23u);
        larvae::AssertTrue(str.Capacity() > wax::String<comb::LinearAllocator>::SsoCapacity);
    });
}
