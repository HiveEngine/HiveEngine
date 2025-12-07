#include <larvae/larvae.h>
#include <queen/scheduler/work_stealing_deque.h>
#include <comb/linear_allocator.h>
#include <thread>
#include <vector>
#include <atomic>

namespace
{
    // ============================================================================
    // CircularBuffer Tests
    // ============================================================================

    auto test1 = larvae::RegisterTest("QueenCircularBuffer", "CreateBuffer", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::CircularBuffer<int, comb::LinearAllocator> buffer{alloc, 16};

        larvae::AssertEqual(buffer.Capacity(), size_t{16});
    });

    auto test2 = larvae::RegisterTest("QueenCircularBuffer", "PutAndGet", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::CircularBuffer<int, comb::LinearAllocator> buffer{alloc, 16};

        buffer.Put(0, 42);
        buffer.Put(1, 100);
        buffer.Put(15, 999);

        larvae::AssertEqual(buffer.Get(0), 42);
        larvae::AssertEqual(buffer.Get(1), 100);
        larvae::AssertEqual(buffer.Get(15), 999);
    });

    auto test3 = larvae::RegisterTest("QueenCircularBuffer", "WrapAround", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::CircularBuffer<int, comb::LinearAllocator> buffer{alloc, 8};

        // With capacity 8, indices 0-7 map to slots 0-7
        // Index 8 wraps to slot 0, index 9 wraps to slot 1, etc.
        buffer.Put(0, 10);
        buffer.Put(7, 77);
        buffer.Put(8, 88);   // Wraps to slot 0, overwriting 10
        buffer.Put(9, 99);   // Wraps to slot 1

        // Index 8 maps to same slot as index 0 (8 & 7 = 0)
        larvae::AssertEqual(buffer.Get(8), 88);
        larvae::AssertEqual(buffer.Get(0), 88);  // Same as index 8
        larvae::AssertEqual(buffer.Get(7), 77);
        larvae::AssertEqual(buffer.Get(9), 99);
        larvae::AssertEqual(buffer.Get(1), 99);  // Same as index 9
    });

    auto test4 = larvae::RegisterTest("QueenCircularBuffer", "Grow", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::CircularBuffer<int, comb::LinearAllocator> buffer{alloc, 4};

        buffer.Put(0, 10);
        buffer.Put(1, 20);
        buffer.Put(2, 30);
        buffer.Put(3, 40);

        auto* grown = buffer.Grow(4, 0);

        larvae::AssertEqual(grown->Capacity(), size_t{8});
        larvae::AssertEqual(grown->Get(0), 10);
        larvae::AssertEqual(grown->Get(1), 20);
        larvae::AssertEqual(grown->Get(2), 30);
        larvae::AssertEqual(grown->Get(3), 40);

        grown->~CircularBuffer<int, comb::LinearAllocator>();
        alloc.Deallocate(grown);
    });

    // ============================================================================
    // WorkStealingDeque Basic Tests
    // ============================================================================

    auto test5 = larvae::RegisterTest("QueenWorkStealingDeque", "CreateDeque", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        larvae::AssertTrue(deque.IsEmpty());
        larvae::AssertEqual(deque.Size(), size_t{0});
    });

    auto test6 = larvae::RegisterTest("QueenWorkStealingDeque", "PushAndPop", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        deque.Push(1);
        deque.Push(2);
        deque.Push(3);

        larvae::AssertFalse(deque.IsEmpty());
        larvae::AssertEqual(deque.Size(), size_t{3});

        auto val3 = deque.Pop();
        larvae::AssertTrue(val3.has_value());
        larvae::AssertEqual(val3.value(), 3);

        auto val2 = deque.Pop();
        larvae::AssertTrue(val2.has_value());
        larvae::AssertEqual(val2.value(), 2);

        auto val1 = deque.Pop();
        larvae::AssertTrue(val1.has_value());
        larvae::AssertEqual(val1.value(), 1);

        larvae::AssertTrue(deque.IsEmpty());
    });

    auto test7 = larvae::RegisterTest("QueenWorkStealingDeque", "PopEmptyReturnsNullopt", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        auto result = deque.Pop();
        larvae::AssertFalse(result.has_value());
    });

    auto test8 = larvae::RegisterTest("QueenWorkStealingDeque", "StealBasic", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        deque.Push(1);
        deque.Push(2);
        deque.Push(3);

        auto stolen = deque.Steal();
        larvae::AssertTrue(stolen.has_value());
        larvae::AssertEqual(stolen.value(), 1);

        larvae::AssertEqual(deque.Size(), size_t{2});
    });

    auto test9 = larvae::RegisterTest("QueenWorkStealingDeque", "StealEmptyReturnsNullopt", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        auto result = deque.Steal();
        larvae::AssertFalse(result.has_value());
    });

    auto test10 = larvae::RegisterTest("QueenWorkStealingDeque", "LIFOBehavior", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        for (int i = 0; i < 5; ++i)
        {
            deque.Push(i);
        }

        for (int i = 4; i >= 0; --i)
        {
            auto val = deque.Pop();
            larvae::AssertTrue(val.has_value());
            larvae::AssertEqual(val.value(), i);
        }
    });

    auto test11 = larvae::RegisterTest("QueenWorkStealingDeque", "FIFOStealBehavior", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        for (int i = 0; i < 5; ++i)
        {
            deque.Push(i);
        }

        for (int i = 0; i < 5; ++i)
        {
            auto stolen = deque.Steal();
            larvae::AssertTrue(stolen.has_value());
            larvae::AssertEqual(stolen.value(), i);
        }
    });

    auto test12 = larvae::RegisterTest("QueenWorkStealingDeque", "MixedPopAndSteal", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        deque.Push(1);
        deque.Push(2);
        deque.Push(3);
        deque.Push(4);

        auto stolen1 = deque.Steal();
        larvae::AssertTrue(stolen1.has_value());
        larvae::AssertEqual(stolen1.value(), 1);

        auto popped4 = deque.Pop();
        larvae::AssertTrue(popped4.has_value());
        larvae::AssertEqual(popped4.value(), 4);

        auto stolen2 = deque.Steal();
        larvae::AssertTrue(stolen2.has_value());
        larvae::AssertEqual(stolen2.value(), 2);

        auto popped3 = deque.Pop();
        larvae::AssertTrue(popped3.has_value());
        larvae::AssertEqual(popped3.value(), 3);

        larvae::AssertTrue(deque.IsEmpty());
    });

    auto test13 = larvae::RegisterTest("QueenWorkStealingDeque", "GrowOnOverflow", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 4};

        for (int i = 0; i < 10; ++i)
        {
            deque.Push(i);
        }

        larvae::AssertEqual(deque.Size(), size_t{10});

        for (int i = 9; i >= 0; --i)
        {
            auto val = deque.Pop();
            larvae::AssertTrue(val.has_value());
            larvae::AssertEqual(val.value(), i);
        }
    });

    auto test14 = larvae::RegisterTest("QueenWorkStealingDeque", "PointerType", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int*, comb::LinearAllocator> deque{alloc, 16};

        int a = 10, b = 20, c = 30;

        deque.Push(&a);
        deque.Push(&b);
        deque.Push(&c);

        auto stolen = deque.Steal();
        larvae::AssertTrue(stolen.has_value());
        larvae::AssertEqual(*stolen.value(), 10);

        auto popped = deque.Pop();
        larvae::AssertTrue(popped.has_value());
        larvae::AssertEqual(*popped.value(), 30);
    });

    // ============================================================================
    // Concurrent Tests
    // ============================================================================

    auto test15 = larvae::RegisterTest("QueenWorkStealingDeque", "SingleProducerMultipleStealers", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 1024};

        constexpr int kNumItems = 10000;
        constexpr int kNumStealers = 4;

        std::atomic<int> total_stolen{0};
        std::atomic<bool> done_producing{false};

        auto stealer_func = [&]()
        {
            int local_count = 0;
            while (!done_producing.load(std::memory_order_acquire) || !deque.IsEmpty())
            {
                auto stolen = deque.Steal();
                if (stolen.has_value())
                {
                    ++local_count;
                }
                else
                {
                    std::this_thread::yield();
                }
            }
            total_stolen.fetch_add(local_count, std::memory_order_relaxed);
        };

        std::vector<std::thread> stealers;
        for (int i = 0; i < kNumStealers; ++i)
        {
            stealers.emplace_back(stealer_func);
        }

        for (int i = 0; i < kNumItems; ++i)
        {
            deque.Push(i);
        }

        done_producing.store(true, std::memory_order_release);

        for (auto& t : stealers)
        {
            t.join();
        }

        larvae::AssertEqual(total_stolen.load(), kNumItems);
    });

    auto test16 = larvae::RegisterTest("QueenWorkStealingDeque", "ProducerConsumerWithStealers", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 1024};

        constexpr int kNumItems = 5000;
        constexpr int kNumStealers = 2;

        std::atomic<int> owner_processed{0};
        std::atomic<int> total_stolen{0};
        std::atomic<bool> done{false};

        auto stealer_func = [&]()
        {
            int local_count = 0;
            while (!done.load(std::memory_order_acquire))
            {
                auto stolen = deque.Steal();
                if (stolen.has_value())
                {
                    ++local_count;
                }
                else
                {
                    std::this_thread::yield();
                }
            }
            while (auto stolen = deque.Steal())
            {
                ++local_count;
            }
            total_stolen.fetch_add(local_count, std::memory_order_relaxed);
        };

        std::vector<std::thread> stealers;
        for (int i = 0; i < kNumStealers; ++i)
        {
            stealers.emplace_back(stealer_func);
        }

        int owner_count = 0;
        for (int i = 0; i < kNumItems; ++i)
        {
            deque.Push(i);

            if (i % 3 == 0)
            {
                auto val = deque.Pop();
                if (val.has_value())
                {
                    ++owner_count;
                }
            }
        }

        while (auto val = deque.Pop())
        {
            ++owner_count;
        }

        owner_processed.store(owner_count, std::memory_order_relaxed);

        done.store(true, std::memory_order_release);

        for (auto& t : stealers)
        {
            t.join();
        }

        int total = owner_processed.load() + total_stolen.load();
        larvae::AssertEqual(total, kNumItems);
    });

    auto test17 = larvae::RegisterTest("QueenWorkStealingDeque", "RaceOnLastItem", []() {
        comb::LinearAllocator alloc{1024 * 1024};

        for (int trial = 0; trial < 100; ++trial)
        {
            queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

            deque.Push(42);

            std::atomic<int> pop_got{0};
            std::atomic<int> steal_got{0};

            std::thread stealer([&]()
            {
                auto result = deque.Steal();
                if (result.has_value())
                {
                    steal_got.store(result.value(), std::memory_order_relaxed);
                }
            });

            auto result = deque.Pop();
            if (result.has_value())
            {
                pop_got.store(result.value(), std::memory_order_relaxed);
            }

            stealer.join();

            int pop_val = pop_got.load();
            int steal_val = steal_got.load();

            bool one_got_it = (pop_val == 42 && steal_val == 0) || (pop_val == 0 && steal_val == 42);
            larvae::AssertTrue(one_got_it);

            alloc.Reset();
        }
    });

    auto test18 = larvae::RegisterTest("QueenWorkStealingDeque", "StressTest", []() {
        comb::LinearAllocator alloc{16 * 1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 64};

        constexpr int kIterations = 10000;

        std::atomic<int> produced{0};
        std::atomic<int> consumed{0};
        std::atomic<bool> done{false};

        auto owner_func = [&]()
        {
            int local_produced = 0;
            int local_consumed = 0;

            for (int i = 0; i < kIterations; ++i)
            {
                deque.Push(i);
                ++local_produced;

                if (i % 2 == 0)
                {
                    auto val = deque.Pop();
                    if (val.has_value())
                    {
                        ++local_consumed;
                    }
                }
            }

            while (auto val = deque.Pop())
            {
                ++local_consumed;
            }

            produced.store(local_produced, std::memory_order_relaxed);
            consumed.fetch_add(local_consumed, std::memory_order_relaxed);
            done.store(true, std::memory_order_release);
        };

        auto stealer_func = [&]()
        {
            int local_stolen = 0;
            while (!done.load(std::memory_order_acquire))
            {
                auto val = deque.Steal();
                if (val.has_value())
                {
                    ++local_stolen;
                }
            }
            while (auto val = deque.Steal())
            {
                ++local_stolen;
            }
            consumed.fetch_add(local_stolen, std::memory_order_relaxed);
        };

        std::thread stealer1(stealer_func);
        std::thread stealer2(stealer_func);
        std::thread owner(owner_func);

        owner.join();
        stealer1.join();
        stealer2.join();

        larvae::AssertEqual(produced.load(), kIterations);
        larvae::AssertEqual(consumed.load(), kIterations);
    });

    auto test19 = larvae::RegisterTest("QueenWorkStealingDeque", "EmptyAfterDrain", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 16};

        for (int i = 0; i < 100; ++i)
        {
            deque.Push(i);
        }

        while (deque.Pop().has_value()) {}

        larvae::AssertTrue(deque.IsEmpty());
        larvae::AssertEqual(deque.Size(), size_t{0});

        larvae::AssertFalse(deque.Pop().has_value());
        larvae::AssertFalse(deque.Steal().has_value());
    });
}
