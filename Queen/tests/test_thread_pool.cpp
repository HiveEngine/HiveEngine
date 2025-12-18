#include <larvae/larvae.h>
#include <queen/scheduler/thread_pool.h>
#include <comb/linear_allocator.h>
#include <atomic>
#include <vector>
#include <chrono>

namespace
{
    // ============================================================================
    // Task Tests
    // ============================================================================

    auto test1 = larvae::RegisterTest("QueenTask", "DefaultConstruction", []() {
        queen::Task task{};
        larvae::AssertFalse(task.IsValid());
    });

    auto test2 = larvae::RegisterTest("QueenTask", "ExecuteValidTask", []() {
        int counter = 0;
        queen::Task task{
            [](void* data) { ++(*static_cast<int*>(data)); },
            &counter
        };

        larvae::AssertTrue(task.IsValid());
        task.Execute();
        larvae::AssertEqual(counter, 1);
    });

    auto test3 = larvae::RegisterTest("QueenTask", "ExecuteInvalidTaskIsNoOp", []() {
        queen::Task task{};
        task.Execute();
    });

    // ============================================================================
    // ThreadPool Basic Tests
    // ============================================================================

    auto test4 = larvae::RegisterTest("QueenThreadPool", "Creation", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        larvae::AssertEqual(pool.WorkerCount(), size_t{4});
        larvae::AssertFalse(pool.IsRunning());
    });

    auto test5 = larvae::RegisterTest("QueenThreadPool", "StartAndStop", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2};

        larvae::AssertFalse(pool.IsRunning());

        pool.Start();
        larvae::AssertTrue(pool.IsRunning());

        pool.Stop();
        larvae::AssertFalse(pool.IsRunning());
    });

    auto test6 = larvae::RegisterTest("QueenThreadPool", "DoubleStartIsNoOp", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2};

        pool.Start();
        pool.Start();
        larvae::AssertTrue(pool.IsRunning());

        pool.Stop();
    });

    auto test7 = larvae::RegisterTest("QueenThreadPool", "DoubleStopIsNoOp", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2};

        pool.Start();
        pool.Stop();
        pool.Stop();

        larvae::AssertFalse(pool.IsRunning());
    });

    auto test8 = larvae::RegisterTest("QueenThreadPool", "DefaultWorkerCount", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc};

        size_t expected = std::thread::hardware_concurrency();
        if (expected == 0) expected = 4;

        larvae::AssertEqual(pool.WorkerCount(), expected);
    });

    // ============================================================================
    // Task Submission Tests
    // ============================================================================

    auto test9 = larvae::RegisterTest("QueenThreadPool", "SubmitSingleTask", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2};

        std::atomic<int> counter{0};

        pool.Start();

        pool.Submit([](void* data) {
            static_cast<std::atomic<int>*>(data)->fetch_add(1);
        }, &counter);

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), 1);
    });

    auto test10 = larvae::RegisterTest("QueenThreadPool", "SubmitMultipleTasks", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        constexpr int kNumTasks = 100;
        std::atomic<int> counter{0};

        pool.Start();

        for (int i = 0; i < kNumTasks; ++i)
        {
            pool.Submit([](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);
        }

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), kNumTasks);
    });

    auto test11 = larvae::RegisterTest("QueenThreadPool", "SubmitToSpecificWorker", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        std::atomic<int> counter{0};

        pool.Start();

        pool.SubmitTo(2, [](void* data) {
            static_cast<std::atomic<int>*>(data)->fetch_add(1);
        }, &counter);

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), 1);
    });

    auto test12 = larvae::RegisterTest("QueenThreadPool", "PendingTaskCount", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2};

        std::atomic<bool> block{true};

        pool.Start();

        // Submit a task that blocks until we release it
        pool.Submit([](void* data) {
            auto* block_ptr = static_cast<std::atomic<bool>*>(data);
            while (block_ptr->load()) {
                std::this_thread::yield();
            }
        }, &block);

        // Small delay to ensure task is picked up
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Task is running but not complete, may or may not show as pending
        // (depends on timing)

        block.store(false);
        pool.WaitAll();

        larvae::AssertEqual(pool.PendingTaskCount(), int64_t{0});

        pool.Stop();
    });

    // ============================================================================
    // Work Stealing Tests
    // ============================================================================

    auto test13 = larvae::RegisterTest("QueenThreadPool", "WorkStealing", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        constexpr int kNumTasks = 1000;
        std::atomic<int> counter{0};

        pool.Start();

        // Submit all tasks to worker 0
        for (int i = 0; i < kNumTasks; ++i)
        {
            pool.SubmitTo(0, [](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
                // Small work to make stealing worthwhile
                volatile int sum = 0;
                for (int j = 0; j < 100; ++j) sum += j;
                (void)sum;
            }, &counter);
        }

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), kNumTasks);
    });

    auto test14 = larvae::RegisterTest("QueenThreadPool", "LoadBalancing", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        constexpr int kNumTasks = 10000;
        std::atomic<int> counter{0};

        pool.Start();

        for (int i = 0; i < kNumTasks; ++i)
        {
            pool.Submit([](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);
        }

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), kNumTasks);
    });

    // ============================================================================
    // Stress Tests
    // ============================================================================

    auto test15 = larvae::RegisterTest("QueenThreadPool", "StressTest", []() {
        comb::LinearAllocator alloc{16 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 8};

        constexpr int kNumTasks = 50000;
        std::atomic<int> counter{0};

        pool.Start();

        for (int i = 0; i < kNumTasks; ++i)
        {
            pool.Submit([](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            }, &counter);
        }

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), kNumTasks);
    });

    auto test16 = larvae::RegisterTest("QueenThreadPool", "ConcurrentSubmit", []() {
        comb::LinearAllocator alloc{16 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};

        constexpr int kNumSubmitters = 4;
        constexpr int kTasksPerSubmitter = 1000;

        std::atomic<int> counter{0};

        pool.Start();

        std::vector<std::thread> submitters;
        for (int s = 0; s < kNumSubmitters; ++s)
        {
            submitters.emplace_back([&pool, &counter]() {
                for (int i = 0; i < kTasksPerSubmitter; ++i)
                {
                    pool.Submit([](void* data) {
                        static_cast<std::atomic<int>*>(data)->fetch_add(1);
                    }, &counter);
                }
            });
        }

        for (auto& t : submitters)
        {
            t.join();
        }

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), kNumSubmitters * kTasksPerSubmitter);
    });

    auto test17 = larvae::RegisterTest("QueenThreadPool", "IdleStrategyYield", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2, queen::IdleStrategy::Yield};

        larvae::AssertTrue(pool.GetIdleStrategy() == queen::IdleStrategy::Yield);

        std::atomic<int> counter{0};

        pool.Start();

        pool.Submit([](void* data) {
            static_cast<std::atomic<int>*>(data)->fetch_add(1);
        }, &counter);

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), 1);
    });

    auto test18 = larvae::RegisterTest("QueenThreadPool", "IdleStrategySpin", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2, queen::IdleStrategy::Spin};

        larvae::AssertTrue(pool.GetIdleStrategy() == queen::IdleStrategy::Spin);

        std::atomic<int> counter{0};

        pool.Start();

        pool.Submit([](void* data) {
            static_cast<std::atomic<int>*>(data)->fetch_add(1);
        }, &counter);

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(counter.load(), 1);
    });

    auto test19 = larvae::RegisterTest("QueenThreadPool", "WorkerStatesTransition", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2};

        // Before start, workers should be Idle (default state)
        larvae::AssertTrue(pool.GetWorkerState(0) == queen::WorkerState::Idle);
        larvae::AssertTrue(pool.GetWorkerState(1) == queen::WorkerState::Idle);

        pool.Start();

        // Give workers time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Workers should be idle or stealing (no tasks)
        auto state0 = pool.GetWorkerState(0);
        auto state1 = pool.GetWorkerState(1);
        larvae::AssertTrue(
            state0 == queen::WorkerState::Idle ||
            state0 == queen::WorkerState::Stealing
        );
        larvae::AssertTrue(
            state1 == queen::WorkerState::Idle ||
            state1 == queen::WorkerState::Stealing
        );

        pool.Stop();

        // After stop, workers should be Stopped
        larvae::AssertTrue(pool.GetWorkerState(0) == queen::WorkerState::Stopped);
        larvae::AssertTrue(pool.GetWorkerState(1) == queen::WorkerState::Stopped);
    });

    auto test20 = larvae::RegisterTest("QueenThreadPool", "InvalidWorkerIndex", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2};

        // Out of bounds index should return Stopped
        larvae::AssertTrue(pool.GetWorkerState(100) == queen::WorkerState::Stopped);
    });

    auto test21 = larvae::RegisterTest("QueenThreadPool", "TaskWithReturn", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 2};

        struct TaskData
        {
            int input;
            std::atomic<int> output;
        };

        TaskData data{42, {0}};

        pool.Start();

        pool.Submit([](void* ptr) {
            auto* d = static_cast<TaskData*>(ptr);
            d->output.store(d->input * 2);
        }, &data);

        pool.WaitAll();
        pool.Stop();

        larvae::AssertEqual(data.output.load(), 84);
    });
}
