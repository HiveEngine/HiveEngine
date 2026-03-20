#include <larvae/larvae.h>

#include <drone/work_stealing_deque.h>

#include <comb/buddy_allocator.h>

#include <atomic>
#include <thread>
#include <vector>

namespace
{
    using TestAlloc = comb::BuddyAllocator;

    // WorkStealingDeque — owner thread

    auto tWS1 = larvae::RegisterTest("DroneWorkStealingDeque", "PushPopSingle", []() {
        TestAlloc alloc{1024 * 1024};
        drone::WorkStealingDeque<int, TestAlloc> deque{alloc, 64};

        deque.Push(42);
        auto val = deque.Pop();
        larvae::AssertTrue(val.has_value());
        larvae::AssertEqual(*val, 42);
    });

    auto tWS2 = larvae::RegisterTest("DroneWorkStealingDeque", "PopEmptyReturnsNullopt", []() {
        TestAlloc alloc{1024 * 1024};
        drone::WorkStealingDeque<int, TestAlloc> deque{alloc, 64};

        auto val = deque.Pop();
        larvae::AssertFalse(val.has_value());
    });

    auto tWS3 = larvae::RegisterTest("DroneWorkStealingDeque", "PopIsLifo", []() {
        TestAlloc alloc{1024 * 1024};
        drone::WorkStealingDeque<int, TestAlloc> deque{alloc, 64};

        deque.Push(1);
        deque.Push(2);
        deque.Push(3);

        auto v3 = deque.Pop();
        auto v2 = deque.Pop();
        auto v1 = deque.Pop();

        larvae::AssertTrue(v3.has_value());
        larvae::AssertTrue(v2.has_value());
        larvae::AssertTrue(v1.has_value());
        larvae::AssertEqual(*v3, 3);
        larvae::AssertEqual(*v2, 2);
        larvae::AssertEqual(*v1, 1);
    });

    auto tWS4 = larvae::RegisterTest("DroneWorkStealingDeque", "StealIsFifo", []() {
        TestAlloc alloc{1024 * 1024};
        drone::WorkStealingDeque<int, TestAlloc> deque{alloc, 64};

        deque.Push(1);
        deque.Push(2);
        deque.Push(3);

        auto v1 = deque.Steal();
        auto v2 = deque.Steal();
        auto v3 = deque.Steal();

        larvae::AssertTrue(v1.has_value());
        larvae::AssertTrue(v2.has_value());
        larvae::AssertTrue(v3.has_value());
        larvae::AssertEqual(*v1, 1);
        larvae::AssertEqual(*v2, 2);
        larvae::AssertEqual(*v3, 3);
    });

    auto tWS5 = larvae::RegisterTest("DroneWorkStealingDeque", "StealEmptyReturnsNullopt", []() {
        TestAlloc alloc{1024 * 1024};
        drone::WorkStealingDeque<int, TestAlloc> deque{alloc, 64};

        auto val = deque.Steal();
        larvae::AssertFalse(val.has_value());
    });

    // WorkStealingDeque — cross-thread steal

    auto tWS6 = larvae::RegisterTest("DroneWorkStealingDeque", "StealFromAnotherThread", []() {
        TestAlloc alloc{2 * 1024 * 1024};
        drone::WorkStealingDeque<int, TestAlloc> deque{alloc, 1024};

        constexpr int kItems = 200;
        for (int i = 0; i < kItems; ++i)
        {
            deque.Push(i);
        }

        std::atomic<int> stolen{0};
        constexpr int kThieves = 4;
        std::vector<std::thread> thieves;
        thieves.reserve(kThieves);

        for (int t = 0; t < kThieves; ++t)
        {
            thieves.emplace_back([&deque, &stolen]() {
                while (auto val = deque.Steal())
                {
                    stolen.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& t : thieves)
        {
            t.join();
        }

        // Owner may also pop remaining items
        int popped = 0;
        while (deque.Pop().has_value())
        {
            ++popped;
        }

        larvae::AssertEqual(stolen.load() + popped, kItems);
    });

    auto tWS7 = larvae::RegisterTest("DroneWorkStealingDeque", "ConcurrentPushAndSteal", []() {
        TestAlloc alloc{2 * 1024 * 1024};
        drone::WorkStealingDeque<int, TestAlloc> deque{alloc, 1024};

        constexpr int kItems = 500;
        std::atomic<int> stolen{0};
        std::atomic<bool> ownerDone{false};

        // Thief thread
        std::thread thief{[&deque, &stolen, &ownerDone]() {
            while (!ownerDone.load(std::memory_order_acquire) || !deque.IsEmpty())
            {
                if (deque.Steal().has_value())
                {
                    stolen.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }};

        // Owner pushes
        for (int i = 0; i < kItems; ++i)
        {
            deque.Push(i);
        }
        ownerDone.store(true, std::memory_order_release);

        thief.join();

        // Drain remaining via Pop
        int popped = 0;
        while (deque.Pop().has_value())
        {
            ++popped;
        }

        larvae::AssertEqual(stolen.load() + popped, kItems);
    });

    auto tWS8 = larvae::RegisterTest("DroneWorkStealingDeque", "SizeTracksCorrectly", []() {
        TestAlloc alloc{1024 * 1024};
        drone::WorkStealingDeque<int, TestAlloc> deque{alloc, 64};

        larvae::AssertEqual(deque.Size(), size_t{0});
        larvae::AssertTrue(deque.IsEmpty());

        deque.Push(1);
        deque.Push(2);
        larvae::AssertEqual(deque.Size(), size_t{2});

        deque.Pop();
        larvae::AssertEqual(deque.Size(), size_t{1});

        deque.Steal();
        larvae::AssertEqual(deque.Size(), size_t{0});
        larvae::AssertTrue(deque.IsEmpty());
    });

} // namespace
