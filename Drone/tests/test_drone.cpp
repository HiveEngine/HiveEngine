#include <comb/buddy_allocator.h>

#include <wax/pointers/arc.h>

#include <drone/counter.h>
#include <drone/job_system.h>
#include <drone/task.h>

#include <larvae/larvae.h>

#include <atomic>

namespace
{
    using TestAlloc = comb::BuddyAllocator;

    struct TestJobSystem
    {
        TestAlloc m_alloc{2 * 1024 * 1024};
        drone::JobSystem<TestAlloc> m_system{m_alloc, {2, 1024, 1024, 64 * 1024}};
        drone::JobSubmitter m_submitter{drone::MakeJobSubmitter(m_system)};

        TestJobSystem()
        {
            m_system.Start();
        }
        ~TestJobSystem()
        {
            m_system.Stop();
        }
    };

    // Counter

    auto t1 = larvae::RegisterTest("DroneCounter", "InitialValueZero", []() {
        drone::Counter counter;
        larvae::AssertTrue(counter.IsDone());
        larvae::AssertEqual(counter.Value(), int64_t{0});
    });

    auto t2 = larvae::RegisterTest("DroneCounter", "AddAndDecrement", []() {
        drone::Counter counter;
        counter.Add(3);
        larvae::AssertEqual(counter.Value(), int64_t{3});
        counter.Decrement();
        larvae::AssertEqual(counter.Value(), int64_t{2});
        counter.Decrement();
        counter.Decrement();
        larvae::AssertTrue(counter.IsDone());
    });

    auto t3 = larvae::RegisterTest("DroneCounter", "WaitReturnsImmediatelyWhenDone", []() {
        drone::Counter counter;
        counter.Wait(); // should not block
        larvae::AssertTrue(counter.IsDone());
    });

    auto t4 = larvae::RegisterTest("DroneCounter", "ExplicitInitialValue", []() {
        drone::Counter counter{5};
        larvae::AssertEqual(counter.Value(), int64_t{5});
        larvae::AssertFalse(counter.IsDone());
    });

    // JobSystem

    auto t5 = larvae::RegisterTest("DroneJobSystem", "SubmitAndWait", []() {
        TestJobSystem js;
        std::atomic<int> result{0};

        drone::Counter counter;
        drone::JobDecl job;
        job.m_func = [](void* data) {
            static_cast<std::atomic<int>*>(data)->store(42);
        };
        job.m_userData = &result;
        js.m_system.Submit(job, counter);

        counter.Wait();
        larvae::AssertEqual(result.load(), 42);
    });

    auto t6 = larvae::RegisterTest("DroneJobSystem", "SubmitBatch", []() {
        TestJobSystem js;
        std::atomic<int> count{0};

        constexpr size_t kBatchSize = 100;
        drone::JobDecl jobs[kBatchSize];
        for (auto& j : jobs)
        {
            j.m_func = [](void* data) {
                static_cast<std::atomic<int>*>(data)->fetch_add(1);
            };
            j.m_userData = &count;
        }

        drone::Counter counter;
        js.m_system.Submit(jobs, kBatchSize, counter);
        counter.Wait();

        larvae::AssertEqual(count.load(), static_cast<int>(kBatchSize));
    });

    auto t7 = larvae::RegisterTest("DroneJobSystem", "ParallelFor", []() {
        TestJobSystem js;

        constexpr size_t kSize = 1000;
        int data[kSize]{};

        js.m_system.ParallelFor(
            0, kSize,
            [](size_t i, void* ud) {
                auto* arr = static_cast<int*>(ud);
                arr[i] = static_cast<int>(i * 2);
            },
            data);

        for (size_t i = 0; i < kSize; ++i)
        {
            larvae::AssertEqual(data[i], static_cast<int>(i * 2));
        }
    });

    auto t8 = larvae::RegisterTest("DroneJobSystem", "WorkerScratchExists", []() {
        TestJobSystem js;
        std::atomic<bool> ok{false};

        drone::Counter counter;
        drone::JobDecl job;
        job.m_func = [](void* data) {
            size_t idx = drone::WorkerContext::CurrentWorkerIndex();
            (void)idx;
            static_cast<std::atomic<bool>*>(data)->store(true);
        };
        job.m_userData = &ok;
        js.m_system.Submit(job, counter);
        counter.Wait();

        larvae::AssertTrue(ok.load());
    });

    auto t9 = larvae::RegisterTest("DroneJobSystem", "Priorities", []() {
        TestJobSystem js;
        std::atomic<int> count{0};

        drone::Counter counter;
        drone::JobDecl high;
        high.m_func = [](void* data) {
            static_cast<std::atomic<int>*>(data)->fetch_add(1);
        };
        high.m_userData = &count;
        high.m_priority = drone::Priority::HIGH;

        drone::JobDecl low;
        low.m_func = [](void* data) {
            static_cast<std::atomic<int>*>(data)->fetch_add(1);
        };
        low.m_userData = &count;
        low.m_priority = drone::Priority::LOW;

        js.m_system.Submit(high, counter);
        js.m_system.Submit(low, counter);
        counter.Wait();

        larvae::AssertEqual(count.load(), 2);
    });

    // Task<T> Coroutine

    drone::Task<int> SimpleCoroutine()
    {
        co_return 42;
    }

    drone::Task<int> ChainedCoroutine()
    {
        int a = co_await SimpleCoroutine();
        int b = co_await SimpleCoroutine();
        co_return a + b;
    }

    drone::Task<void> VoidCoroutine(std::atomic<bool>& flag)
    {
        flag.store(true);
        co_return;
    }

    auto t10 = larvae::RegisterTest("DroneTask", "SimpleTask", []() {
        auto task = SimpleCoroutine();
        int result = drone::SyncWait(task);
        larvae::AssertEqual(result, 42);
    });

    auto t11 = larvae::RegisterTest("DroneTask", "ChainedTask", []() {
        auto task = ChainedCoroutine();
        int result = drone::SyncWait(task);
        larvae::AssertEqual(result, 84);
    });

    auto t12 = larvae::RegisterTest("DroneTask", "VoidTask", []() {
        std::atomic<bool> flag{false};
        auto task = VoidCoroutine(flag);
        drone::SyncWait(task);
        larvae::AssertTrue(flag.load());
    });

    // Arc<T>

    auto t13 = larvae::RegisterTest("WaxArc", "BasicUsage", []() {
        auto arc = wax::MakeArc<int>(42);
        larvae::AssertEqual(*arc, 42);
        larvae::AssertEqual(arc.GetRefCount(), size_t{1});
    });

    auto t14 = larvae::RegisterTest("WaxArc", "CopyIncrementsRefCount", []() {
        auto a = wax::MakeArc<int>(10);
        auto b = a;
        larvae::AssertEqual(a.GetRefCount(), size_t{2});
        larvae::AssertEqual(b.GetRefCount(), size_t{2});
        larvae::AssertEqual(*a, *b);
    });

    auto t15 = larvae::RegisterTest("WaxArc", "MoveTransfersOwnership", []() {
        auto a = wax::MakeArc<int>(99);
        auto b = static_cast<wax::Arc<int>&&>(a);
        larvae::AssertTrue(a.IsNull());
        larvae::AssertEqual(*b, 99);
        larvae::AssertEqual(b.GetRefCount(), size_t{1});
    });

    auto t16 = larvae::RegisterTest("WaxArc", "CrossThreadSharing", []() {
        TestJobSystem js;
        auto arc = wax::MakeArc<std::atomic<int>>(0);

        constexpr int kJobs = 100;
        drone::Counter counter;

        struct Ctx
        {
            wax::Arc<std::atomic<int>>* m_arc;
        };
        Ctx ctx{&arc};

        for (int i = 0; i < kJobs; ++i)
        {
            drone::JobDecl job;
            job.m_func = [](void* data) {
                auto& arc = *static_cast<Ctx*>(data)->m_arc;
                arc->fetch_add(1);
            };
            job.m_userData = &ctx;
            js.m_system.Submit(job, counter);
        }

        counter.Wait();
        larvae::AssertEqual(arc->load(), kJobs);
    });

} // namespace
