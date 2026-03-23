#pragma once

#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>
#include <comb/linear_allocator.h>
#include <comb/thread_safe_allocator.h>

#include <wax/containers/vector.h>

#include <drone/counter.h>
#include <drone/parallel_scratch.h>
#include <drone/job_submitter.h>
#include <drone/job_types.h>
#include <drone/mpmc_queue.h>
#include <drone/work_stealing_deque.h>
#include <drone/worker_context.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace drone
{
    struct JobSystemConfig
    {
        size_t m_workerCount = 0;               // 0 = hardware_concurrency - 1
        size_t m_dequeCapacity = 4096;
        size_t m_globalCapacity = 4096;
        size_t m_scratchSize = 2 * 1024 * 1024; // 2 MB per worker
    };

    template <comb::Allocator Allocator> class JobSystem
    {
    public:
        using SafeAllocator = comb::ThreadSafeAllocator<Allocator>;

        explicit JobSystem(Allocator& allocator, const JobSystemConfig& config = {})
            : m_allocator{&allocator}
            , m_safeAllocator{allocator}
            , m_workerCount{config.m_workerCount == 0 ? DefaultWorkerCount() : config.m_workerCount}
            , m_running{false}
            , m_shouldStop{false}
        {
            void* workerMem = m_allocator->Allocate(sizeof(WorkerData) * m_workerCount, alignof(WorkerData));
            m_workers = static_cast<WorkerData*>(workerMem);
            for (size_t i = 0; i < m_workerCount; ++i)
            {
                new (&m_workers[i]) WorkerData{};
                m_workers[i].m_id = i;
                m_workers[i].m_rngState = static_cast<uint32_t>(i + 1);

                void* dequeMem = m_allocator->Allocate(sizeof(WorkStealingDeque<JobDecl, SafeAllocator>),
                                                       alignof(WorkStealingDeque<JobDecl, SafeAllocator>));
                m_workers[i].m_deque =
                    new (dequeMem) WorkStealingDeque<JobDecl, SafeAllocator>{m_safeAllocator, config.m_dequeCapacity};
            }

            for (size_t p = 0; p < static_cast<size_t>(Priority::COUNT); ++p)
            {
                void* qMem = m_allocator->Allocate(sizeof(MPMCQueue<JobDecl, Allocator>),
                                                   alignof(MPMCQueue<JobDecl, Allocator>));
                m_globalQueues[p] = new (qMem) MPMCQueue<JobDecl, Allocator>{allocator, config.m_globalCapacity};
            }

            size_t totalScratch = m_workerCount + 1;
            m_scratchAllocators = static_cast<comb::LinearAllocator*>(
                m_allocator->Allocate(sizeof(comb::LinearAllocator) * totalScratch, alignof(comb::LinearAllocator)));
            for (size_t i = 0; i < totalScratch; ++i)
            {
                new (&m_scratchAllocators[i]) comb::LinearAllocator{config.m_scratchSize};
            }
            m_scratchCount = totalScratch;
        }

        ~JobSystem()
        {
            Stop();

            for (size_t i = 0; i < m_scratchCount; ++i)
            {
                m_scratchAllocators[i].~LinearAllocator();
            }
            m_allocator->Deallocate(m_scratchAllocators);

            for (size_t p = 0; p < static_cast<size_t>(Priority::COUNT); ++p)
            {
                m_globalQueues[p]->~MPMCQueue<JobDecl, Allocator>();
                m_allocator->Deallocate(m_globalQueues[p]);
            }

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                m_workers[i].m_deque->~WorkStealingDeque<JobDecl, SafeAllocator>();
                m_allocator->Deallocate(m_workers[i].m_deque);
                m_workers[i].~WorkerData();
            }
            m_allocator->Deallocate(m_workers);
        }

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;
        JobSystem(JobSystem&&) = delete;
        JobSystem& operator=(JobSystem&&) = delete;

        void Start()
        {
            if (m_running.exchange(true))
                return;

            m_shouldStop.store(false, std::memory_order_relaxed);

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                m_workers[i].m_thread = std::thread(&JobSystem::WorkerMain, this, i);
            }
        }

        void Stop()
        {
            if (!m_running.exchange(false))
                return;

            m_shouldStop.store(true, std::memory_order_release);
            m_parkCv.notify_all();

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                if (m_workers[i].m_thread.joinable())
                {
                    m_workers[i].m_thread.join();
                }
            }
        }

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return m_running.load(std::memory_order_acquire);
        }

        // --- Submission ---

        void Submit(const JobDecl* jobs, size_t count, Counter& counter)
        {
            counter.Add(static_cast<int64_t>(count));
            for (size_t i = 0; i < count; ++i)
            {
                JobDecl tagged = jobs[i];
                tagged.m_counter = &counter;
                SubmitInternal(tagged);
            }
        }

        void Submit(JobDecl job, Counter& counter)
        {
            counter.Add(1);
            job.m_counter = &counter;
            SubmitInternal(job);
        }

        void SubmitDetached(JobDecl job)
        {
            job.m_counter = nullptr;
            SubmitInternal(job);
        }

        // --- ParallelFor ---

        void ParallelFor(size_t begin, size_t end, void (*func)(size_t index, void* data), void* data,
                         size_t chunkSize = 0)
        {
            if (begin >= end)
                return;

            const size_t total = end - begin;
            if (chunkSize == 0)
            {
                chunkSize = (total + m_workerCount) / (m_workerCount + 1);
                if (chunkSize == 0)
                    chunkSize = 1;
            }
            const size_t numChunks = (total + chunkSize - 1) / chunkSize;

            hive::Assert(numChunks <= kMaxParallelChunks, "ParallelFor: too many chunks");

            ParallelChunk* chunks = GetParallelChunkBuffer();
            size_t baseIdx = AllocateParallelChunkSlots(numChunks);

            Counter counter{static_cast<int64_t>(numChunks)};

            for (size_t chunk = 0; chunk < numChunks; ++chunk)
            {
                const size_t chunkBegin = begin + chunk * chunkSize;
                const size_t chunkEnd = (chunkBegin + chunkSize < end) ? (chunkBegin + chunkSize) : end;

                auto& cd = chunks[(baseIdx + chunk) % kMaxParallelChunks];
                cd.m_func = func;
                cd.m_userData = data;
                cd.m_begin = chunkBegin;
                cd.m_end = chunkEnd;
                cd.m_counter = &counter;

                JobDecl job;
                job.m_func = [](void* d) {
                    auto* pc = static_cast<ParallelChunk*>(d);
                    for (size_t i = pc->m_begin; i < pc->m_end; ++i)
                    {
                        pc->m_func(i, pc->m_userData);
                    }
                    pc->m_counter->Decrement();
                };
                job.m_userData = &cd;
                job.m_priority = Priority::HIGH;

                SubmitInternal(job);
            }

            counter.Wait();
        }

        // --- Worker info ---

        [[nodiscard]] size_t WorkerCount() const noexcept
        {
            return m_workerCount;
        }

        [[nodiscard]] comb::LinearAllocator& WorkerScratch()
        {
            size_t idx = WorkerContext::CurrentWorkerIndex();
            if (idx == WorkerContext::kMainThread)
                return m_scratchAllocators[0];
            return m_scratchAllocators[idx + 1];
        }

        [[nodiscard]] comb::LinearAllocator& WorkerScratch(size_t workerIndex)
        {
            return m_scratchAllocators[workerIndex + 1];
        }

        [[nodiscard]] comb::LinearAllocator& MainScratch()
        {
            return m_scratchAllocators[0];
        }

        void ResetAllScratch()
        {
            for (size_t i = 0; i < m_scratchCount; ++i)
            {
                m_scratchAllocators[i].Reset();
            }
        }

    private:
        struct WorkerData
        {
            size_t m_id{0};
            std::thread m_thread;
            WorkStealingDeque<JobDecl, SafeAllocator>* m_deque{nullptr};
            uint32_t m_rngState{0};

            WorkerData() = default;
            ~WorkerData() = default;
            WorkerData(const WorkerData&) = delete;
            WorkerData& operator=(const WorkerData&) = delete;
            WorkerData(WorkerData&&) = delete;
            WorkerData& operator=(WorkerData&&) = delete;
        };

        static size_t DefaultWorkerCount() noexcept
        {
            size_t count = std::thread::hardware_concurrency();
            return count > 1 ? count - 1 : 1;
        }

        static uint32_t XorShift32(uint32_t& state) noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        void SubmitInternal(const JobDecl& job)
        {
            HIVE_PROFILE_SCOPE_N("JobSystem::Submit");

            auto prio = static_cast<size_t>(job.m_priority);
            m_globalQueues[prio]->Push(job);
            m_parkCv.notify_one();
        }

        [[nodiscard]] bool TryExecuteOne(size_t workerIdx)
        {
            auto* deque = m_workers[workerIdx].m_deque;

            // 1. Local deque (LIFO — cache hot)
            if (auto local = deque->Pop())
            {
                ExecuteJob(local.value());
                return true;
            }

            // 2. Global queues by priority
            for (size_t p = 0; p < static_cast<size_t>(Priority::COUNT); ++p)
            {
                if (auto global = m_globalQueues[p]->Pop())
                {
                    ExecuteJob(global.value());
                    return true;
                }
            }

            // 3. Steal from random victim
            uint32_t start = XorShift32(m_workers[workerIdx].m_rngState) % static_cast<uint32_t>(m_workerCount);
            for (size_t i = 0; i < m_workerCount; ++i)
            {
                size_t victim = (static_cast<size_t>(start) + i) % m_workerCount;
                if (victim == workerIdx)
                    continue;

                if (auto stolen = m_workers[victim].m_deque->Steal())
                {
                    ExecuteJob(stolen.value());
                    return true;
                }
            }

            return false;
        }

        void ExecuteJob(const JobDecl& job)
        {
            HIVE_PROFILE_SCOPE_N("JobExecute");
            job.Execute();
        }

        void WorkerMain(size_t workerIdx)
        {
            char threadName[32];
            std::snprintf(threadName, sizeof(threadName), "Drone %zu", workerIdx);
            HIVE_PROFILE_THREAD(threadName);

            WorkerContext::SetCurrentWorkerIndex(workerIdx);

            uint32_t idleSpins = 0;

            while (!m_shouldStop.load(std::memory_order_acquire))
            {
                if (TryExecuteOne(workerIdx))
                {
                    idleSpins = 0;
                }
                else
                {
                    IdleBackoff(idleSpins);
                }
            }

            while (TryExecuteOne(workerIdx))
            {
            }

            WorkerContext::ClearCurrentWorkerIndex();
        }

        void IdleBackoff(uint32_t& spinCount)
        {
            if (spinCount < 64)
            {
                std::atomic_signal_fence(std::memory_order_seq_cst);
                ++spinCount;
            }
            else if (spinCount < 128)
            {
                std::this_thread::yield();
                ++spinCount;
            }
            else
            {
                std::unique_lock lock{m_parkMutex};
                m_parkCv.wait_for(lock, std::chrono::microseconds(500),
                                  [this] { return m_shouldStop.load(std::memory_order_relaxed) || HasWork(); });
                spinCount = 0;
            }
        }

        [[nodiscard]] bool HasWork() const noexcept
        {
            for (size_t p = 0; p < static_cast<size_t>(Priority::COUNT); ++p)
            {
                if (!m_globalQueues[p]->IsEmpty())
                    return true;
            }
            return false;
        }

        Allocator* m_allocator;
        SafeAllocator m_safeAllocator;

        WorkerData* m_workers;
        size_t m_workerCount;

        MPMCQueue<JobDecl, Allocator>* m_globalQueues[static_cast<size_t>(Priority::COUNT)]{};

        comb::LinearAllocator* m_scratchAllocators{nullptr};
        size_t m_scratchCount{0};

        std::mutex m_parkMutex;
        std::condition_variable m_parkCv;

        std::atomic<bool> m_running;
        std::atomic<bool> m_shouldStop;
    };

    template <comb::Allocator Allocator> JobSubmitter MakeJobSubmitter(JobSystem<Allocator>& system)
    {
        return JobSubmitter{
            &system,
            [](void* ctx, const JobDecl* jobs, size_t count, Counter* counter) {
                auto& sys = *static_cast<JobSystem<Allocator>*>(ctx);
                if (counter != nullptr)
                {
                    sys.Submit(jobs, count, *counter);
                }
                else
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        sys.SubmitDetached(jobs[i]);
                    }
                }
            },
            [](void* ctx, size_t begin, size_t end, void (*func)(size_t, void*), void* data, size_t chunkSize) {
                auto& sys = *static_cast<JobSystem<Allocator>*>(ctx);
                sys.ParallelFor(begin, end, func, data, chunkSize);
            },
            [](void* ctx) -> size_t {
                auto& sys = *static_cast<JobSystem<Allocator>*>(ctx);
                return sys.WorkerCount();
            },
        };
    }
} // namespace drone
