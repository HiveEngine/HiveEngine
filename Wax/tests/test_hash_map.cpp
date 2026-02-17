#include <larvae/larvae.h>
#include <wax/containers/hash_map.h>
#include <comb/linear_allocator.h>
#include <comb/buddy_allocator.h>

namespace
{
    auto test1 = larvae::RegisterTest("WaxHashMap", "InsertAndFind", []() {
        comb::LinearAllocator alloc{8192};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        bool inserted = map.Insert(42, 100);
        larvae::AssertTrue(inserted);
        larvae::AssertEqual(map.Count(), size_t{1});

        int* value = map.Find(42);
        larvae::AssertNotNull(value);
        larvae::AssertEqual(*value, 100);
    });

    auto test2 = larvae::RegisterTest("WaxHashMap", "FindNotFound", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(1, 100);

        int* found = map.Find(999);
        larvae::AssertNull(found);
    });

    auto test3 = larvae::RegisterTest("WaxHashMap", "DuplicateKey", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        bool first = map.Insert(42, 100);
        bool second = map.Insert(42, 200);

        larvae::AssertTrue(first);
        larvae::AssertFalse(second);
        larvae::AssertEqual(map.Count(), size_t{1});
        larvae::AssertEqual(*map.Find(42), 100);
    });

    auto test4 = larvae::RegisterTest("WaxHashMap", "Remove", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(1, 10);
        map.Insert(2, 20);
        map.Insert(3, 30);

        larvae::AssertEqual(map.Count(), size_t{3});

        bool removed = map.Remove(2);
        larvae::AssertTrue(removed);
        larvae::AssertEqual(map.Count(), size_t{2});
        larvae::AssertNull(map.Find(2));

        larvae::AssertNotNull(map.Find(1));
        larvae::AssertNotNull(map.Find(3));
    });

    auto test5 = larvae::RegisterTest("WaxHashMap", "RemoveNotFound", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(1, 10);

        bool removed = map.Remove(999);
        larvae::AssertFalse(removed);
        larvae::AssertEqual(map.Count(), size_t{1});
    });

    auto test6 = larvae::RegisterTest("WaxHashMap", "Clear", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(1, 10);
        map.Insert(2, 20);
        map.Insert(3, 30);

        map.Clear();

        larvae::AssertEqual(map.Count(), size_t{0});
        larvae::AssertTrue(map.IsEmpty());
        larvae::AssertNull(map.Find(1));
        larvae::AssertNull(map.Find(2));
        larvae::AssertNull(map.Find(3));
    });

    auto test7 = larvae::RegisterTest("WaxHashMap", "Contains", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(42, 100);

        larvae::AssertTrue(map.Contains(42));
        larvae::AssertFalse(map.Contains(999));
    });

    auto test8 = larvae::RegisterTest("WaxHashMap", "Rehash", []() {
        comb::BuddyAllocator alloc{65536};
        wax::HashMap<int, int, comb::BuddyAllocator> map{alloc, 4};

        for (int i = 0; i < 100; ++i)
        {
            map.Insert(i, i * 10);
        }

        larvae::AssertEqual(map.Count(), size_t{100});

        for (int i = 0; i < 100; ++i)
        {
            int* value = map.Find(i);
            larvae::AssertNotNull(value);
            larvae::AssertEqual(*value, i * 10);
        }
    });

    auto test9 = larvae::RegisterTest("WaxHashMap", "Iterator", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(1, 10);
        map.Insert(2, 20);
        map.Insert(3, 30);

        int sum_keys = 0;
        int sum_values = 0;
        int count = 0;

        for (auto it = map.begin(); it != map.end(); ++it)
        {
            sum_keys += it.Key();
            sum_values += it.Value();
            ++count;
        }

        larvae::AssertEqual(count, 3);
        larvae::AssertEqual(sum_keys, 6);
        larvae::AssertEqual(sum_values, 60);
    });

    auto test10 = larvae::RegisterTest("WaxHashMap", "OperatorBracket", []() {
        comb::BuddyAllocator alloc{8192};
        wax::HashMap<int, int, comb::BuddyAllocator> map{alloc, 16};

        map.Insert(42, 100);

        larvae::AssertEqual(map[42], 100);

        map[42] = 200;
        larvae::AssertEqual(map[42], 200);
    });

    auto test11 = larvae::RegisterTest("WaxHashMap", "FloatValues", []() {
        comb::BuddyAllocator alloc{16384};

        wax::HashMap<int, float, comb::BuddyAllocator> map{alloc, 16};

        map.Insert(1, 1.5f);
        map.Insert(2, 2.5f);
        map.Insert(3, 3.5f);

        larvae::AssertEqual(*map.Find(1), 1.5f);
        larvae::AssertEqual(*map.Find(2), 2.5f);
        larvae::AssertEqual(*map.Find(3), 3.5f);
    });

    auto test12 = larvae::RegisterTest("WaxHashMap", "MoveConstruct", []() {
        comb::LinearAllocator alloc{8192};
        wax::HashMap<int, int, comb::LinearAllocator> map1{alloc, 16};

        map1.Insert(1, 10);
        map1.Insert(2, 20);

        wax::HashMap<int, int, comb::LinearAllocator> map2{static_cast<wax::HashMap<int, int, comb::LinearAllocator>&&>(map1)};

        larvae::AssertEqual(map2.Count(), size_t{2});
        larvae::AssertNotNull(map2.Find(1));
        larvae::AssertNotNull(map2.Find(2));
    });

    auto test13 = larvae::RegisterTest("WaxHashMap", "RemoveAndReinsert", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(1, 10);
        map.Insert(2, 20);
        map.Remove(1);

        bool reinserted = map.Insert(1, 100);
        larvae::AssertTrue(reinserted);
        larvae::AssertEqual(*map.Find(1), 100);
    });

    struct NonTrivialValue
    {
        static int destructor_count;
        int value;

        NonTrivialValue(int v = 0) : value{v} {}
        ~NonTrivialValue() { ++destructor_count; }
        NonTrivialValue(const NonTrivialValue& other) : value{other.value} {}
        NonTrivialValue(NonTrivialValue&& other) noexcept : value{other.value} { other.value = 0; }
        NonTrivialValue& operator=(const NonTrivialValue& other) { value = other.value; return *this; }
        NonTrivialValue& operator=(NonTrivialValue&& other) noexcept { value = other.value; other.value = 0; return *this; }
    };

    int NonTrivialValue::destructor_count = 0;

    auto test14 = larvae::RegisterTest("WaxHashMap", "DestructorsCalled", []() {
        NonTrivialValue::destructor_count = 0;

        {
            comb::LinearAllocator alloc{4096};
            wax::HashMap<int, NonTrivialValue, comb::LinearAllocator> map{alloc, 16};

            map.Emplace(1, 10);
            map.Emplace(2, 20);
            map.Emplace(3, 30);

            int before_remove = NonTrivialValue::destructor_count;
            map.Remove(2);
            larvae::AssertTrue(NonTrivialValue::destructor_count > before_remove);
        }

        larvae::AssertTrue(NonTrivialValue::destructor_count >= 3);
    });

    auto test15 = larvae::RegisterTest("WaxHashMap", "Emplace", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, NonTrivialValue, comb::LinearAllocator> map{alloc, 16};

        bool inserted = map.Emplace(42, 100);
        larvae::AssertTrue(inserted);

        auto* found = map.Find(42);
        larvae::AssertNotNull(found);
        larvae::AssertEqual(found->value, 100);
    });

    auto test16 = larvae::RegisterTest("WaxHashMap", "MoveAssignment", []() {
        comb::BuddyAllocator alloc{16384};
        wax::HashMap<int, int, comb::BuddyAllocator> map1{alloc, 16};
        wax::HashMap<int, int, comb::BuddyAllocator> map2{alloc, 16};

        map1.Insert(1, 10);
        map1.Insert(2, 20);

        map2.Insert(100, 1000);

        map2 = static_cast<wax::HashMap<int, int, comb::BuddyAllocator>&&>(map1);

        larvae::AssertEqual(map2.Count(), size_t{2});
        larvae::AssertNotNull(map2.Find(1));
        larvae::AssertEqual(*map2.Find(1), 10);
        larvae::AssertNotNull(map2.Find(2));
        larvae::AssertEqual(*map2.Find(2), 20);
        larvae::AssertFalse(map2.Contains(100));
    });

    auto test17 = larvae::RegisterTest("WaxHashMap", "EmptyMapIteration", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        int count = 0;
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            ++count;
        }

        larvae::AssertEqual(count, 0);
    });

    auto test18 = larvae::RegisterTest("WaxHashMap", "OperatorBracketDefaultConstruct", []() {
        comb::BuddyAllocator alloc{8192};
        wax::HashMap<int, int, comb::BuddyAllocator> map{alloc, 16};

        // Access missing key creates default value
        int& val = map[99];
        larvae::AssertEqual(val, 0);
        larvae::AssertEqual(map.Count(), size_t{1});
        larvae::AssertTrue(map.Contains(99));

        // Modify through reference
        val = 42;
        larvae::AssertEqual(map[99], 42);
    });

    auto test19 = larvae::RegisterTest("WaxHashMap", "ConstFind", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(1, 10);
        map.Insert(2, 20);

        const auto& const_map = map;

        const int* found = const_map.Find(1);
        larvae::AssertNotNull(found);
        larvae::AssertEqual(*found, 10);

        const int* not_found = const_map.Find(999);
        larvae::AssertNull(not_found);
    });

    auto test20 = larvae::RegisterTest("WaxHashMap", "RangeForLoop", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashMap<int, int, comb::LinearAllocator> map{alloc, 16};

        map.Insert(1, 10);
        map.Insert(2, 20);
        map.Insert(3, 30);

        int sum_keys = 0;
        int sum_values = 0;
        for (const auto& [key, value] : map)
        {
            sum_keys += key;
            sum_values += value;
        }

        larvae::AssertEqual(sum_keys, 6);
        larvae::AssertEqual(sum_values, 60);
    });
}
