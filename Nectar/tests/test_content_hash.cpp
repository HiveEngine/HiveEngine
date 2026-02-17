#include <larvae/larvae.h>
#include <nectar/core/content_hash.h>
#include <wax/serialization/byte_span.h>
#include <cstring>

namespace {

    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }

    // =========================================================================
    // Construction
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarContentHash", "DefaultConstructIsInvalid", []() {
        nectar::ContentHash h{};
        larvae::AssertFalse(h.IsValid());
        larvae::AssertEqual(h.High(), uint64_t{0});
        larvae::AssertEqual(h.Low(), uint64_t{0});
    });

    auto t2 = larvae::RegisterTest("NectarContentHash", "ExplicitConstruct", []() {
        nectar::ContentHash h{42, 99};
        larvae::AssertTrue(h.IsValid());
        larvae::AssertEqual(h.High(), uint64_t{42});
        larvae::AssertEqual(h.Low(), uint64_t{99});
    });

    auto t3 = larvae::RegisterTest("NectarContentHash", "InvalidIsSameAsDefault", []() {
        auto inv = nectar::ContentHash::Invalid();
        nectar::ContentHash def{};
        larvae::AssertTrue(inv == def);
        larvae::AssertFalse(inv.IsValid());
    });

    // =========================================================================
    // FromData
    // =========================================================================

    auto t4 = larvae::RegisterTest("NectarContentHash", "FromDataProducesNonZero", []() {
        const char* data = "hello";
        auto h = nectar::ContentHash::FromData(data, 5);
        larvae::AssertTrue(h.IsValid());
    });

    auto t5 = larvae::RegisterTest("NectarContentHash", "FromDataDeterministic", []() {
        const char* data = "hello world";
        auto h1 = nectar::ContentHash::FromData(data, 11);
        auto h2 = nectar::ContentHash::FromData(data, 11);
        larvae::AssertTrue(h1 == h2);
    });

    auto t6 = larvae::RegisterTest("NectarContentHash", "FromDataDifferentInputs", []() {
        auto h1 = nectar::ContentHash::FromData("hello", 5);
        auto h2 = nectar::ContentHash::FromData("world", 5);
        larvae::AssertTrue(h1 != h2);
    });

    auto t7 = larvae::RegisterTest("NectarContentHash", "FromEmptyDataIsDeterministic", []() {
        auto h1 = nectar::ContentHash::FromData(nullptr, 0);
        auto h2 = nectar::ContentHash::FromData(nullptr, 0);
        larvae::AssertTrue(h1 == h2);
        // Empty data is valid content (not Invalid)
        larvae::AssertTrue(h1.IsValid());
    });

    auto t8 = larvae::RegisterTest("NectarContentHash", "FromByteSpan", []() {
        uint8_t data[] = {1, 2, 3, 4, 5};
        wax::ByteSpan span{data, 5};
        auto h1 = nectar::ContentHash::FromData(span);
        auto h2 = nectar::ContentHash::FromData(data, 5);
        larvae::AssertTrue(h1 == h2);
    });

    auto t9 = larvae::RegisterTest("NectarContentHash", "FromSingleByte", []() {
        uint8_t a = 0x00;
        uint8_t b = 0xFF;
        auto h1 = nectar::ContentHash::FromData(&a, 1);
        auto h2 = nectar::ContentHash::FromData(&b, 1);
        larvae::AssertTrue(h1 != h2);
    });

    auto t10 = larvae::RegisterTest("NectarContentHash", "FromLargeData", []() {
        constexpr size_t kSize = 64_KB;
        uint8_t data[kSize];
        std::memset(data, 0xAB, kSize);
        auto h = nectar::ContentHash::FromData(data, kSize);
        larvae::AssertTrue(h.IsValid());

        // Flip one byte
        data[kSize / 2] = 0xCD;
        auto h2 = nectar::ContentHash::FromData(data, kSize);
        larvae::AssertTrue(h != h2);
    });

    // =========================================================================
    // Operators
    // =========================================================================

    auto t11 = larvae::RegisterTest("NectarContentHash", "EqualityOperator", []() {
        nectar::ContentHash a{100, 200};
        nectar::ContentHash b{100, 200};
        nectar::ContentHash c{100, 201};
        larvae::AssertTrue(a == b);
        larvae::AssertFalse(a == c);
        larvae::AssertFalse(a != b);
        larvae::AssertTrue(a != c);
    });

    auto t12 = larvae::RegisterTest("NectarContentHash", "LessThanOperator", []() {
        nectar::ContentHash a{1, 0};
        nectar::ContentHash b{2, 0};
        nectar::ContentHash c{1, 1};
        larvae::AssertTrue(a < b);
        larvae::AssertFalse(b < a);
        larvae::AssertTrue(a < c);
    });

    // =========================================================================
    // Hash and ToString
    // =========================================================================

    auto t13 = larvae::RegisterTest("NectarContentHash", "HashForHashMap", []() {
        auto h = nectar::ContentHash::FromData("test", 4);
        size_t hash = h.Hash();
        // Just verify it doesn't crash and returns something
        larvae::AssertTrue(hash != 0 || h.High() == h.Low());
    });

    auto t14 = larvae::RegisterTest("NectarContentHash", "ToStringLength", []() {
        auto h = nectar::ContentHash::FromData("abc", 3);
        auto str = h.ToString();
        larvae::AssertEqual(str.Size(), size_t{32});
    });

    auto t15 = larvae::RegisterTest("NectarContentHash", "ToStringHexCharsOnly", []() {
        nectar::ContentHash h{0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL};
        auto str = h.ToString();
        const char* s = str.CStr();
        for (size_t i = 0; i < 32; ++i)
        {
            bool valid = (s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f');
            larvae::AssertTrue(valid);
        }
    });

    auto t16 = larvae::RegisterTest("NectarContentHash", "StdHashSpecialization", []() {
        nectar::ContentHash h{42, 99};
        std::hash<nectar::ContentHash> hasher;
        size_t result = hasher(h);
        larvae::AssertEqual(result, h.Hash());
    });

}
