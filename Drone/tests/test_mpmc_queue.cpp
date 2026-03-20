#include <larvae/larvae.h>

#include <drone/mpmc_queue.h>

#include <comb/buddy_allocator.h>

#include <atomic>
#include <thread>
#include <vector>

namespace
{
    using TestAlloc = comb::BuddyAllocator;

    // MPMCQueue — single-threaded

    auto tMQ1 = larvae::RegisterTest("DroneMpmcQueue", "PushPopSingle", []() {
        TestAlloc alloc{1024 * 1024};
        drone::MPMCQueue<int, TestAlloc> q{alloc, 16};

        larvae::AssertTrue(q.Push(42));
        auto val = q.Pop();
        larvae::AssertTrue(val.has_value());
        larvae::AssertEqual(*val, 42);
    });

    auto tMQ2 = larvae::RegisterTest("DroneMpmcQueue", "PushPopMultiple", []() {
        TestAlloc alloc{1024 * 1024};
        drone::MPMCQueue<int, TestAlloc> q{alloc, 16};

        for (int i = 0; i < 10; ++i)
        {
            larvae::AssertTrue(q.Push(i));
        }
        larvae::AssertEqual(q.Size(), size_t{10});

        for (int i = 0; i < 10; ++i)
        {
            auto val = q.Pop();
            larvae::AssertTrue(val.has_value());
            larvae::AssertEqual(*val, i);
        }
        larvae::AssertTrue(q.IsEmpty());
    });

    auto tMQ3 = larvae::RegisterTest("DroneMpmcQueue", "PopEmptyReturnsNullopt", []() {
        TestAlloc alloc{1024 * 1024};
        drone::MPMCQueue<int, TestAlloc> q{alloc, 16};

        auto val = q.Pop();
        larvae::AssertFalse(val.has_value());
        larvae::AssertTrue(q.IsEmpty());
    });

    auto tMQ4 = larvae::RegisterTest("DroneMpmcQueue", "FullQueueReturnsFalse", []() {
        TestAlloc alloc{1024 * 1024};
        // Capacity rounds up to power of 2, so request 4 -> get 4
        drone::MPMCQueue<int, TestAlloc> q{alloc, 4};

        for (size_t i = 0; i < q.Capacity(); ++i)
        {
            larvae::AssertTrue(q.Push(static_cast<int>(i)));
        }
        // Queue is full — next push should fail
        larvae::AssertFalse(q.Push(999));
    });

    auto tMQ5 = larvae::RegisterTest("DroneMpmcQueue", "CapacityRoundsUpPow2", []() {
        TestAlloc alloc{1024 * 1024};
        drone::MPMCQueue<int, TestAlloc> q{alloc, 5};
        larvae::AssertEqual(q.Capacity(), size_t{8});
    });

    // MPMCQueue — concurrent

    auto tMQ6 = larvae::RegisterTest("DroneMpmcQueue", "ConcurrentPushSinglePop", []() {
        TestAlloc alloc{2 * 1024 * 1024};
        drone::MPMCQueue<int, TestAlloc> q{alloc, 1024};

        constexpr int kProducers = 4;
        constexpr int kItemsPerProducer = 100;

        std::vector<std::thread> producers;
        producers.reserve(kProducers);

        for (int p = 0; p < kProducers; ++p)
        {
            producers.emplace_back([&q, p]() {
                for (int i = 0; i < kItemsPerProducer; ++i)
                {
                    while (!q.Push(p * kItemsPerProducer + i))
                    {
                        // Retry on full
                    }
                }
            });
        }

        for (auto& t : producers)
        {
            t.join();
        }

        // Pop all from single thread
        int count = 0;
        while (auto val = q.Pop())
        {
            ++count;
        }
        larvae::AssertEqual(count, kProducers * kItemsPerProducer);
    });

    auto tMQ7 = larvae::RegisterTest("DroneMpmcQueue", "ConcurrentPushPop", []() {
        TestAlloc alloc{2 * 1024 * 1024};
        drone::MPMCQueue<int, TestAlloc> q{alloc, 256};

        constexpr int kProducers = 4;
        constexpr int kConsumers = 4;
        constexpr int kItemsPerProducer = 200;

        std::atomic<int> produced{0};
        std::atomic<int> consumed{0};

        std::vector<std::thread> threads;
        threads.reserve(kProducers + kConsumers);

        // Producers
        for (int p = 0; p < kProducers; ++p)
        {
            threads.emplace_back([&q, &produced]() {
                for (int i = 0; i < kItemsPerProducer; ++i)
                {
                    while (!q.Push(i))
                    {
                    }
                    produced.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        // Consumers
        std::atomic<bool> done{false};
        for (int c = 0; c < kConsumers; ++c)
        {
            threads.emplace_back([&q, &consumed, &done]() {
                while (!done.load(std::memory_order_acquire))
                {
                    if (q.Pop().has_value())
                    {
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                // Drain remaining
                while (q.Pop().has_value())
                {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        // Wait for producers
        for (int i = 0; i < kProducers; ++i)
        {
            threads[static_cast<size_t>(i)].join();
        }

        // Signal consumers to finish
        done.store(true, std::memory_order_release);
        for (int i = kProducers; i < kProducers + kConsumers; ++i)
        {
            threads[static_cast<size_t>(i)].join();
        }

        larvae::AssertEqual(produced.load(), kProducers * kItemsPerProducer);
        larvae::AssertEqual(consumed.load(), kProducers * kItemsPerProducer);
    });

} // namespace
