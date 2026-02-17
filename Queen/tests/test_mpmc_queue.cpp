#include <larvae/larvae.h>
#include <queen/scheduler/mpmc_queue.h>
#include <comb/linear_allocator.h>
#include <atomic>
#include <thread>
#include <vector>

namespace
{
    // ============================================================================
    // Basic Construction Tests
    // ============================================================================

    auto test1 = larvae::RegisterTest("QueenMPMCQueue", "Creation", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 16};

        larvae::AssertTrue(queue.IsEmpty());
        larvae::AssertEqual(queue.Size(), size_t{0});
        larvae::AssertEqual(queue.Capacity(), size_t{16});
    });

    auto test2 = larvae::RegisterTest("QueenMPMCQueue", "CapacityRoundsUpToPowerOf2", []() {
        comb::LinearAllocator alloc{1024 * 1024};

        queen::MPMCQueue<int, comb::LinearAllocator> q1{alloc, 3};
        larvae::AssertEqual(q1.Capacity(), size_t{4});

        queen::MPMCQueue<int, comb::LinearAllocator> q2{alloc, 5};
        larvae::AssertEqual(q2.Capacity(), size_t{8});

        queen::MPMCQueue<int, comb::LinearAllocator> q3{alloc, 7};
        larvae::AssertEqual(q3.Capacity(), size_t{8});

        queen::MPMCQueue<int, comb::LinearAllocator> q4{alloc, 8};
        larvae::AssertEqual(q4.Capacity(), size_t{8});

        queen::MPMCQueue<int, comb::LinearAllocator> q5{alloc, 1};
        larvae::AssertEqual(q5.Capacity(), size_t{1});
    });

    // ============================================================================
    // Single-Thread Push/Pop Tests
    // ============================================================================

    auto test3 = larvae::RegisterTest("QueenMPMCQueue", "PushAndPopSingle", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 16};

        larvae::AssertTrue(queue.Push(42));
        larvae::AssertFalse(queue.IsEmpty());
        larvae::AssertEqual(queue.Size(), size_t{1});

        auto result = queue.Pop();
        larvae::AssertTrue(result.has_value());
        larvae::AssertEqual(result.value(), 42);
        larvae::AssertTrue(queue.IsEmpty());
    });

    auto test4 = larvae::RegisterTest("QueenMPMCQueue", "PushAndPopMultiple", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 16};

        for (int i = 0; i < 10; ++i)
        {
            larvae::AssertTrue(queue.Push(i));
        }

        larvae::AssertEqual(queue.Size(), size_t{10});

        for (int i = 0; i < 10; ++i)
        {
            auto result = queue.Pop();
            larvae::AssertTrue(result.has_value());
            larvae::AssertEqual(result.value(), i);
        }

        larvae::AssertTrue(queue.IsEmpty());
    });

    auto test5 = larvae::RegisterTest("QueenMPMCQueue", "PopFromEmptyReturnsNullopt", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 8};

        auto result = queue.Pop();
        larvae::AssertFalse(result.has_value());
    });

    auto test6 = larvae::RegisterTest("QueenMPMCQueue", "PushToFullReturnsFalse", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 4};

        larvae::AssertTrue(queue.Push(1));
        larvae::AssertTrue(queue.Push(2));
        larvae::AssertTrue(queue.Push(3));
        larvae::AssertTrue(queue.Push(4));

        // Queue is full
        larvae::AssertFalse(queue.Push(5));
        larvae::AssertEqual(queue.Size(), size_t{4});
    });

    auto test7 = larvae::RegisterTest("QueenMPMCQueue", "FIFOOrder", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 8};

        queue.Push(10);
        queue.Push(20);
        queue.Push(30);

        larvae::AssertEqual(queue.Pop().value(), 10);
        larvae::AssertEqual(queue.Pop().value(), 20);
        larvae::AssertEqual(queue.Pop().value(), 30);
    });

    auto test8 = larvae::RegisterTest("QueenMPMCQueue", "PushPopCycles", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 4};

        // Fill and drain multiple times to exercise wraparound
        for (int cycle = 0; cycle < 10; ++cycle)
        {
            for (int i = 0; i < 4; ++i)
            {
                larvae::AssertTrue(queue.Push(cycle * 4 + i));
            }

            for (int i = 0; i < 4; ++i)
            {
                auto result = queue.Pop();
                larvae::AssertTrue(result.has_value());
                larvae::AssertEqual(result.value(), cycle * 4 + i);
            }

            larvae::AssertTrue(queue.IsEmpty());
        }
    });

    auto test9 = larvae::RegisterTest("QueenMPMCQueue", "InterleavedPushPop", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 4};

        queue.Push(1);
        queue.Push(2);
        larvae::AssertEqual(queue.Pop().value(), 1);

        queue.Push(3);
        larvae::AssertEqual(queue.Pop().value(), 2);
        larvae::AssertEqual(queue.Pop().value(), 3);

        larvae::AssertTrue(queue.IsEmpty());
    });

    // ============================================================================
    // Struct Element Tests
    // ============================================================================

    auto test10 = larvae::RegisterTest("QueenMPMCQueue", "WithStructType", []() {
        struct Data
        {
            int x;
            float y;
        };

        comb::LinearAllocator alloc{1024 * 1024};
        queen::MPMCQueue<Data, comb::LinearAllocator> queue{alloc, 8};

        queue.Push(Data{42, 3.14f});
        queue.Push(Data{100, 2.71f});

        auto r1 = queue.Pop();
        larvae::AssertTrue(r1.has_value());
        larvae::AssertEqual(r1.value().x, 42);

        auto r2 = queue.Pop();
        larvae::AssertTrue(r2.has_value());
        larvae::AssertEqual(r2.value().x, 100);
    });

    // ============================================================================
    // Multi-Thread Tests
    // ============================================================================

    auto test11 = larvae::RegisterTest("QueenMPMCQueue", "SingleProducerSingleConsumer", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 1024};

        constexpr int kCount = 10000;
        std::atomic<int> sum{0};

        std::thread producer([&queue]() {
            for (int i = 1; i <= kCount; ++i)
            {
                while (!queue.Push(i))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&queue, &sum]() {
            int consumed = 0;
            while (consumed < kCount)
            {
                if (auto val = queue.Pop())
                {
                    sum.fetch_add(val.value(), std::memory_order_relaxed);
                    ++consumed;
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();

        // Sum of 1..kCount = kCount * (kCount + 1) / 2
        int expected = kCount * (kCount + 1) / 2;
        larvae::AssertEqual(sum.load(), expected);
    });

    auto test12 = larvae::RegisterTest("QueenMPMCQueue", "MultiProducerSingleConsumer", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 1024};

        constexpr int kProducers = 4;
        constexpr int kItemsPerProducer = 2500;
        std::atomic<int> total_consumed{0};

        std::vector<std::thread> producers;
        for (int p = 0; p < kProducers; ++p)
        {
            producers.emplace_back([&queue]() {
                for (int i = 0; i < kItemsPerProducer; ++i)
                {
                    while (!queue.Push(1))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        std::thread consumer([&queue, &total_consumed]() {
            int target = kProducers * kItemsPerProducer;
            while (total_consumed.load(std::memory_order_relaxed) < target)
            {
                if (auto val = queue.Pop())
                {
                    total_consumed.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });

        for (auto& t : producers)
        {
            t.join();
        }
        consumer.join();

        larvae::AssertEqual(total_consumed.load(), kProducers * kItemsPerProducer);
    });

    auto test13 = larvae::RegisterTest("QueenMPMCQueue", "SingleProducerMultiConsumer", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 1024};

        constexpr int kTotal = 10000;
        constexpr int kConsumers = 4;
        std::atomic<int> total_consumed{0};

        std::thread producer([&queue]() {
            for (int i = 0; i < kTotal; ++i)
            {
                while (!queue.Push(1))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::vector<std::thread> consumers;
        for (int c = 0; c < kConsumers; ++c)
        {
            consumers.emplace_back([&queue, &total_consumed]() {
                while (total_consumed.load(std::memory_order_relaxed) < kTotal)
                {
                    if (auto val = queue.Pop())
                    {
                        total_consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        producer.join();
        for (auto& t : consumers)
        {
            t.join();
        }

        larvae::AssertEqual(total_consumed.load(), kTotal);
    });

    auto test14 = larvae::RegisterTest("QueenMPMCQueue", "MultiProducerMultiConsumer", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 1024};

        constexpr int kProducers = 4;
        constexpr int kConsumers = 4;
        constexpr int kItemsPerProducer = 2500;
        constexpr int kTotalItems = kProducers * kItemsPerProducer;

        std::atomic<int> total_consumed{0};

        std::vector<std::thread> producers;
        for (int p = 0; p < kProducers; ++p)
        {
            producers.emplace_back([&queue]() {
                for (int i = 0; i < kItemsPerProducer; ++i)
                {
                    while (!queue.Push(1))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        std::vector<std::thread> consumers;
        for (int c = 0; c < kConsumers; ++c)
        {
            consumers.emplace_back([&queue, &total_consumed]() {
                while (total_consumed.load(std::memory_order_relaxed) < kTotalItems)
                {
                    if (auto val = queue.Pop())
                    {
                        total_consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (auto& t : producers)
        {
            t.join();
        }
        for (auto& t : consumers)
        {
            t.join();
        }

        larvae::AssertEqual(total_consumed.load(), kTotalItems);
    });

    // ============================================================================
    // Stress Tests
    // ============================================================================

    auto test15 = larvae::RegisterTest("QueenMPMCQueue", "StressSmallQueue", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        // Small queue forces lots of contention and full/empty transitions
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 4};

        constexpr int kTotal = 10000;
        std::atomic<int> total_consumed{0};

        std::thread producer([&queue]() {
            for (int i = 0; i < kTotal; ++i)
            {
                while (!queue.Push(i))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&queue, &total_consumed]() {
            int consumed = 0;
            while (consumed < kTotal)
            {
                if (queue.Pop().has_value())
                {
                    ++consumed;
                }
                else
                {
                    std::this_thread::yield();
                }
            }
            total_consumed.store(consumed, std::memory_order_relaxed);
        });

        producer.join();
        consumer.join();

        larvae::AssertEqual(total_consumed.load(), kTotal);
    });

    auto test16 = larvae::RegisterTest("QueenMPMCQueue", "NoDuplicateOrLostItems", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::MPMCQueue<int, comb::LinearAllocator> queue{alloc, 256};

        constexpr int kProducers = 4;
        constexpr int kItemsPerProducer = 1000;
        constexpr int kTotalItems = kProducers * kItemsPerProducer;

        // Each producer pushes unique values, consumers track what they got
        std::atomic<int> consumed_count{0};
        std::atomic<int64_t> consumed_sum{0};

        std::vector<std::thread> producers;
        for (int p = 0; p < kProducers; ++p)
        {
            producers.emplace_back([&queue, p]() {
                int base = p * kItemsPerProducer;
                for (int i = 0; i < kItemsPerProducer; ++i)
                {
                    while (!queue.Push(base + i))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        constexpr int kConsumers = 4;
        std::vector<std::thread> consumers;
        for (int c = 0; c < kConsumers; ++c)
        {
            consumers.emplace_back([&queue, &consumed_count, &consumed_sum]() {
                while (consumed_count.load(std::memory_order_relaxed) < kTotalItems)
                {
                    if (auto val = queue.Pop())
                    {
                        consumed_sum.fetch_add(val.value(), std::memory_order_relaxed);
                        consumed_count.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (auto& t : producers) t.join();
        for (auto& t : consumers) t.join();

        // Expected sum: sum of 0..kTotalItems-1 = kTotalItems * (kTotalItems - 1) / 2
        int64_t expected_sum = static_cast<int64_t>(kTotalItems) * (kTotalItems - 1) / 2;

        larvae::AssertEqual(consumed_count.load(), kTotalItems);
        larvae::AssertEqual(consumed_sum.load(), expected_sum);
    });
}
