#include <larvae/larvae.h>
#include <wax/containers/hash_set.h>
#include <comb/linear_allocator.h>
#include <comb/buddy_allocator.h>

namespace
{
    auto test1 = larvae::RegisterTest("WaxHashSet", "InsertAndContains", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        bool inserted = set.Insert(42);
        larvae::AssertTrue(inserted);
        larvae::AssertEqual(set.Count(), size_t{1});
        larvae::AssertTrue(set.Contains(42));
    });

    auto test2 = larvae::RegisterTest("WaxHashSet", "ContainsNotFound", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        set.Insert(1);

        larvae::AssertFalse(set.Contains(999));
    });

    auto test3 = larvae::RegisterTest("WaxHashSet", "DuplicateInsert", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        bool first = set.Insert(42);
        bool second = set.Insert(42);

        larvae::AssertTrue(first);
        larvae::AssertFalse(second);
        larvae::AssertEqual(set.Count(), size_t{1});
    });

    auto test4 = larvae::RegisterTest("WaxHashSet", "Remove", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        set.Insert(1);
        set.Insert(2);
        set.Insert(3);

        larvae::AssertEqual(set.Count(), size_t{3});

        bool removed = set.Remove(2);
        larvae::AssertTrue(removed);
        larvae::AssertEqual(set.Count(), size_t{2});
        larvae::AssertFalse(set.Contains(2));

        larvae::AssertTrue(set.Contains(1));
        larvae::AssertTrue(set.Contains(3));
    });

    auto test5 = larvae::RegisterTest("WaxHashSet", "RemoveNotFound", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        set.Insert(1);

        bool removed = set.Remove(999);
        larvae::AssertFalse(removed);
        larvae::AssertEqual(set.Count(), size_t{1});
    });

    auto test6 = larvae::RegisterTest("WaxHashSet", "Clear", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        set.Insert(1);
        set.Insert(2);
        set.Insert(3);

        set.Clear();

        larvae::AssertEqual(set.Count(), size_t{0});
        larvae::AssertTrue(set.IsEmpty());
        larvae::AssertFalse(set.Contains(1));
        larvae::AssertFalse(set.Contains(2));
        larvae::AssertFalse(set.Contains(3));
    });

    auto test7 = larvae::RegisterTest("WaxHashSet", "Rehash", []() {
        comb::BuddyAllocator alloc{65536};
        wax::HashSet<int, comb::BuddyAllocator> set{alloc, 4};

        for (int i = 0; i < 100; ++i)
        {
            set.Insert(i);
        }

        larvae::AssertEqual(set.Count(), size_t{100});

        for (int i = 0; i < 100; ++i)
        {
            larvae::AssertTrue(set.Contains(i));
        }
    });

    auto test8 = larvae::RegisterTest("WaxHashSet", "Iterator", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        set.Insert(1);
        set.Insert(2);
        set.Insert(3);

        int sum = 0;
        int count = 0;

        for (auto it = set.begin(); it != set.end(); ++it)
        {
            sum += *it;
            ++count;
        }

        larvae::AssertEqual(count, 3);
        larvae::AssertEqual(sum, 6);
    });

    auto test9 = larvae::RegisterTest("WaxHashSet", "MoveConstruct", []() {
        comb::LinearAllocator alloc{8192};
        wax::HashSet<int, comb::LinearAllocator> set1{alloc, 16};

        set1.Insert(1);
        set1.Insert(2);

        wax::HashSet<int, comb::LinearAllocator> set2{static_cast<wax::HashSet<int, comb::LinearAllocator>&&>(set1)};

        larvae::AssertEqual(set2.Count(), size_t{2});
        larvae::AssertTrue(set2.Contains(1));
        larvae::AssertTrue(set2.Contains(2));
    });

    auto test10 = larvae::RegisterTest("WaxHashSet", "RemoveAndReinsert", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        set.Insert(1);
        set.Insert(2);
        set.Remove(1);

        bool reinserted = set.Insert(1);
        larvae::AssertTrue(reinserted);
        larvae::AssertTrue(set.Contains(1));
    });

    struct NonTrivialKey
    {
        static int destructor_count;
        int value;

        NonTrivialKey(int v = 0) : value{v} {}
        ~NonTrivialKey() { ++destructor_count; }
        NonTrivialKey(const NonTrivialKey& other) : value{other.value} {}
        NonTrivialKey(NonTrivialKey&& other) noexcept : value{other.value} { other.value = 0; }
        NonTrivialKey& operator=(const NonTrivialKey& other) { value = other.value; return *this; }
        NonTrivialKey& operator=(NonTrivialKey&& other) noexcept { value = other.value; other.value = 0; return *this; }
        bool operator==(const NonTrivialKey& other) const { return value == other.value; }
    };

    int NonTrivialKey::destructor_count = 0;

    struct NonTrivialKeyHash
    {
        size_t operator()(const NonTrivialKey& k) const { return std::hash<int>{}(k.value); }
    };

    auto test11 = larvae::RegisterTest("WaxHashSet", "DestructorsCalled", []() {
        NonTrivialKey::destructor_count = 0;

        {
            comb::LinearAllocator alloc{4096};
            wax::HashSet<NonTrivialKey, comb::LinearAllocator, NonTrivialKeyHash> set{alloc, 16};

            set.Insert(NonTrivialKey{1});
            set.Insert(NonTrivialKey{2});
            set.Insert(NonTrivialKey{3});

            int before_remove = NonTrivialKey::destructor_count;
            set.Remove(NonTrivialKey{2});
            larvae::AssertTrue(NonTrivialKey::destructor_count > before_remove);
        }

        larvae::AssertTrue(NonTrivialKey::destructor_count >= 3);
    });

    auto test12 = larvae::RegisterTest("WaxHashSet", "LoadFactor", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        larvae::AssertEqual(set.LoadFactor(), 0.0f);

        set.Insert(1);
        set.Insert(2);
        set.Insert(3);
        set.Insert(4);

        float expected = 4.0f / 16.0f;
        larvae::AssertEqual(set.LoadFactor(), expected);
    });

    auto test13 = larvae::RegisterTest("WaxHashSet", "MoveAssignment", []() {
        comb::BuddyAllocator alloc{16384};
        wax::HashSet<int, comb::BuddyAllocator> set1{alloc, 16};
        wax::HashSet<int, comb::BuddyAllocator> set2{alloc, 16};

        set1.Insert(1);
        set1.Insert(2);
        set1.Insert(3);

        set2.Insert(100);

        set2 = static_cast<wax::HashSet<int, comb::BuddyAllocator>&&>(set1);

        larvae::AssertEqual(set2.Count(), size_t{3});
        larvae::AssertTrue(set2.Contains(1));
        larvae::AssertTrue(set2.Contains(2));
        larvae::AssertTrue(set2.Contains(3));
        larvae::AssertFalse(set2.Contains(100));
    });

    auto test14 = larvae::RegisterTest("WaxHashSet", "EmptySetIteration", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        int count = 0;
        for (auto it = set.begin(); it != set.end(); ++it)
        {
            ++count;
        }

        larvae::AssertEqual(count, 0);
    });

    auto test15 = larvae::RegisterTest("WaxHashSet", "ConstIteration", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        set.Insert(10);
        set.Insert(20);
        set.Insert(30);

        const auto& const_set = set;

        int sum = 0;
        int count = 0;
        for (auto it = const_set.begin(); it != const_set.end(); ++it)
        {
            sum += *it;
            ++count;
        }

        larvae::AssertEqual(count, 3);
        larvae::AssertEqual(sum, 60);
    });

    auto test16 = larvae::RegisterTest("WaxHashSet", "RangeForLoop", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<int, comb::LinearAllocator> set{alloc, 16};

        set.Insert(5);
        set.Insert(10);
        set.Insert(15);

        int sum = 0;
        for (int val : set)
        {
            sum += val;
        }

        larvae::AssertEqual(sum, 30);
    });
}
