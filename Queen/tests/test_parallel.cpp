#include <larvae/larvae.h>
#include <queen/scheduler/parallel.h>
#include <comb/linear_allocator.h>
#include <atomic>
#include <vector>

namespace
{
    // ============================================================================
    // WaitGroup Tests
    // ============================================================================

    auto test1 = larvae::RegisterTest("QueenWaitGroup", "DefaultConstruction", []() {
        queen::WaitGroup wg;
        larvae::AssertEqual(wg.Count(), int64_t{0});
        larvae::AssertTrue(wg.IsDone());
    });

    auto test2 = larvae::RegisterTest("QueenWaitGroup", "AddAndDone", []() {
        queen::WaitGroup wg;

        wg.Add(3);
        larvae::AssertEqual(wg.Count(), int64_t{3});
        larvae::AssertFalse(wg.IsDone());

        wg.Done();
        larvae::AssertEqual(wg.Count(), int64_t{2});

        wg.Done();
        wg.Done();
        larvae::AssertEqual(wg.Count(), int64_t{0});
        larvae::AssertTrue(wg.IsDone());
    });

    auto test3 = larvae::RegisterTest("QueenWaitGroup", "WaitReturnsImmediatelyWhenDone", []() {
        queen::WaitGroup wg;
        wg.Wait();  // Should return immediately since count is 0
        larvae::AssertTrue(wg.IsDone());
    });

    auto test4 = larvae::RegisterTest("QueenWaitGroup", "WaitWithTasks", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        queen::WaitGroup wg;
        std::atomic<int> counter{0};

        pool.Start();

        wg.Add(5);
        for (int i = 0; i < 5; ++i)
        {
            struct TaskData
            {
                std::atomic<int>* counter;
                queen::WaitGroup* wg;
            };

            thread_local TaskData tasks[16];
            thread_local size_t idx = 0;

            auto& td = tasks[idx % 16];
            idx++;

            td.counter = &counter;
            td.wg = &wg;

            pool.Submit([](void* data) {
                auto* td = static_cast<TaskData*>(data);
                td->counter->fetch_add(1);
                td->wg->Done();
            }, &td);
        }

        wg.Wait();

        larvae::AssertEqual(counter.load(), 5);
        larvae::AssertTrue(wg.IsDone());

        pool.Stop();
    });

    // ============================================================================
    // parallel_for Tests
    // ============================================================================

    auto test5 = larvae::RegisterTest("QueenParallelFor", "EmptyRange", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        std::atomic<int> counter{0};

        queen::parallel_for(pool, 0, 0,
            [](size_t, void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);

        larvae::AssertEqual(counter.load(), 0);

        pool.Stop();
    });

    auto test6 = larvae::RegisterTest("QueenParallelFor", "SingleElement", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        std::atomic<int> counter{0};

        queen::parallel_for(pool, 0, 1,
            [](size_t, void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);

        larvae::AssertEqual(counter.load(), 1);

        pool.Stop();
    });

    auto test7 = larvae::RegisterTest("QueenParallelFor", "MultipleElements", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        constexpr size_t kCount = 100;
        std::atomic<int> counter{0};

        queen::parallel_for(pool, 0, kCount,
            [](size_t, void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);

        larvae::AssertEqual(counter.load(), static_cast<int>(kCount));

        pool.Stop();
    });

    auto test8 = larvae::RegisterTest("QueenParallelFor", "CustomChunkSize", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        constexpr size_t kCount = 100;
        std::atomic<int> counter{0};

        // Use chunk size of 10
        queen::parallel_for(pool, 0, kCount,
            [](size_t, void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter, 10);

        larvae::AssertEqual(counter.load(), static_cast<int>(kCount));

        pool.Stop();
    });

    auto test9 = larvae::RegisterTest("QueenParallelFor", "NonZeroStart", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        std::atomic<size_t> sum{0};

        // Sum indices from 10 to 20
        queen::parallel_for(pool, 10, 20,
            [](size_t idx, void* data) {
                static_cast<std::atomic<size_t>*>(data)->fetch_add(idx);
            }, &sum);

        // Sum of 10+11+12+...+19 = 145
        larvae::AssertEqual(sum.load(), size_t{145});

        pool.Stop();
    });

    auto test10 = larvae::RegisterTest("QueenParallelFor", "LargeRange", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 8};

        pool.Start();

        constexpr size_t kCount = 10000;
        std::atomic<int> counter{0};

        queen::parallel_for(pool, 0, kCount,
            [](size_t, void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);

        larvae::AssertEqual(counter.load(), static_cast<int>(kCount));

        pool.Stop();
    });

    auto test11 = larvae::RegisterTest("QueenParallelFor", "ModifyArray", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        constexpr size_t kCount = 100;
        int data[kCount] = {0};

        queen::parallel_for(pool, 0, kCount,
            [](size_t idx, void* ud) {
                auto* arr = static_cast<int*>(ud);
                arr[idx] = static_cast<int>(idx * 2);
            }, data);

        // Verify results
        for (size_t i = 0; i < kCount; ++i)
        {
            larvae::AssertEqual(data[i], static_cast<int>(i * 2));
        }

        pool.Stop();
    });

    // ============================================================================
    // parallel_for_each Tests
    // ============================================================================

    auto test12 = larvae::RegisterTest("QueenParallelForEach", "BasicUsage", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        constexpr size_t kCount = 50;
        std::atomic<int> counter{0};

        queen::parallel_for_each(pool, 0, kCount,
            [](size_t, void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);

        larvae::AssertEqual(counter.load(), static_cast<int>(kCount));

        pool.Stop();
    });

    // ============================================================================
    // TaskBatch Tests
    // ============================================================================

    auto test13 = larvae::RegisterTest("QueenTaskBatch", "DefaultConstruction", []() {
        queen::TaskBatch batch;
        larvae::AssertTrue(batch.IsDone());
        larvae::AssertEqual(batch.PendingCount(), int64_t{0});
    });

    auto test14 = larvae::RegisterTest("QueenTaskBatch", "SubmitAndWait", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        queen::TaskBatch batch;
        std::atomic<int> counter{0};

        for (int i = 0; i < 5; ++i)
        {
            batch.Submit(pool, [](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);
        }

        batch.Wait();

        larvae::AssertEqual(counter.load(), 5);
        larvae::AssertTrue(batch.IsDone());

        pool.Stop();
    });

    auto test15 = larvae::RegisterTest("QueenTaskBatch", "MultipleBatches", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        pool.Start();

        queen::TaskBatch batch1;
        queen::TaskBatch batch2;

        std::atomic<int> counter1{0};
        std::atomic<int> counter2{0};

        for (int i = 0; i < 3; ++i)
        {
            batch1.Submit(pool, [](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter1);
        }

        for (int i = 0; i < 5; ++i)
        {
            batch2.Submit(pool, [](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter2);
        }

        batch1.Wait();
        batch2.Wait();

        larvae::AssertEqual(counter1.load(), 3);
        larvae::AssertEqual(counter2.load(), 5);

        pool.Stop();
    });

    auto test16 = larvae::RegisterTest("QueenTaskBatch", "StressTest", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 8};

        pool.Start();

        queen::TaskBatch batch;
        std::atomic<int> counter{0};

        constexpr int kNumTasks = 500;

        for (int i = 0; i < kNumTasks; ++i)
        {
            batch.Submit(pool, [](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);
        }

        batch.Wait();

        larvae::AssertEqual(counter.load(), kNumTasks);

        pool.Stop();
    });
}
