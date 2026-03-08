#pragma once

#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>

#include <queen/core/tick.h>
#include <queen/scheduler/dependency_graph.h>
#include <queen/scheduler/parallel.h>
#include <queen/scheduler/thread_pool.h>
#include <queen/system/system_storage.h>

#include <atomic>
#include <cstring>

namespace queen
{
    class World;

    /**
     * Parallel scheduler for ECS systems
     *
     * ParallelScheduler executes independent systems in parallel using a
     * work-stealing thread pool. Systems with conflicting data access
     * are serialized to ensure correctness.
     *
     * Use cases:
     * - Multi-threaded system execution
     * - Scaling ECS across multiple cores
     * - Maximum throughput for independent systems
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ graph_: DependencyGraph (system dependencies)                   │
     * │ pool_: ThreadPool* (worker threads)                             │
     * │ owns_pool_: bool (whether we created the pool)                  │
     * │ remaining_: atomic<size_t>* (per-node remaining deps)           │
     * │ remaining_count_: size_t (size of remaining array)              │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Algorithm:
     * 1. Reset dependency counts for all nodes
     * 2. Submit root systems (no dependencies) to thread pool
     * 3. When a system completes, decrement dependency counts of dependents
     * 4. When a dependent's count reaches 0, submit it to thread pool
     * 5. Wait for all systems to complete
     * 6. Flush command buffers
     *
     * Performance characteristics:
     * - Build: O(N^2) where N = number of systems
     * - Update: O(N/P) with P workers for independent systems
     * - Parallel speedup depends on system graph structure
     *
     * Limitations:
     * - Systems must be thread-safe
     * - Command buffers are flushed after all systems (sync point)
     * - Overhead from scheduling may not help for trivial systems
     */
    template <comb::Allocator Allocator> class ParallelScheduler
    {
    public:
        /**
         * Create a ParallelScheduler with an existing thread pool
         */
        explicit ParallelScheduler(Allocator& allocator, ThreadPool<Allocator>& pool)
            : m_graph{allocator}
            , m_pool{&pool}
            , m_ownsPool{false}
            , m_remaining{nullptr}
            , m_remainingCount{0}
            , m_allocator{&allocator} {}

        /**
         * Create a ParallelScheduler with a new internal thread pool
         */
        explicit ParallelScheduler(Allocator& allocator, size_t workerCount = 0)
            : m_graph{allocator}
            , m_pool{nullptr}
            , m_ownsPool{true}
            , m_remaining{nullptr}
            , m_remainingCount{0}
            , m_allocator{&allocator} {
            void* poolMem = m_allocator->Allocate(sizeof(ThreadPool<Allocator>), alignof(ThreadPool<Allocator>));
            m_pool = new (poolMem) ThreadPool<Allocator>{allocator, workerCount};
            m_pool->Start();
        }

        ~ParallelScheduler() {
            if (m_remaining != nullptr)
            {
                for (size_t i = 0; i < m_remainingCount; ++i)
                {
                    m_remaining[i].~atomic<uint16_t>();
                }
                m_allocator->Deallocate(m_remaining);
            }

            if (m_ownsPool && m_pool != nullptr)
            {
                m_pool->Stop();
                m_pool->~ThreadPool<Allocator>();
                m_allocator->Deallocate(m_pool);
            }
        }

        ParallelScheduler(const ParallelScheduler&) = delete;
        ParallelScheduler& operator=(const ParallelScheduler&) = delete;
        ParallelScheduler(ParallelScheduler&&) = delete;
        ParallelScheduler& operator=(ParallelScheduler&&) = delete;

        /**
         * Build/rebuild the dependency graph from system storage
         */
        void Build(const SystemStorage<Allocator>& storage) {
            m_graph.Build(storage);
            ReallocateRemaining(m_graph.NodeCount());
        }

        /**
         * Mark the graph as needing rebuild
         */
        void Invalidate() noexcept { m_graph.MarkDirty(); }

        /**
         * Check if the graph needs rebuild
         */
        [[nodiscard]] bool NeedsRebuild() const noexcept { return m_graph.IsDirty(); }

        /**
         * Run all systems in parallel where possible
         *
         * Independent systems are executed concurrently. Dependent systems
         * wait for their dependencies to complete before executing.
         */
        void RunAll(World& world, SystemStorage<Allocator>& storage); // Implementation in parallel_scheduler_impl.h

        /**
         * Get the dependency graph
         */
        [[nodiscard]] const DependencyGraph<Allocator>& Graph() const noexcept { return m_graph; }

        [[nodiscard]] DependencyGraph<Allocator>& Graph() noexcept { return m_graph; }

        /**
         * Get the thread pool
         */
        [[nodiscard]] ThreadPool<Allocator>* Pool() noexcept { return m_pool; }

        /**
         * Get the execution order (for debugging/visualization)
         */
        [[nodiscard]] const wax::Vector<uint32_t>& ExecutionOrder() const noexcept { return m_graph.ExecutionOrder(); }

        /**
         * Check if the dependency graph has cycles
         */
        [[nodiscard]] bool HasCycle() const noexcept { return m_graph.HasCycle(); }

    private:
        void ReallocateRemaining(size_t count) {
            // Clean up old array
            if (m_remaining != nullptr)
            {
                for (size_t i = 0; i < m_remainingCount; ++i)
                {
                    m_remaining[i].~atomic<uint16_t>();
                }
                m_allocator->Deallocate(m_remaining);
                m_remaining = nullptr;
            }

            m_remainingCount = count;
            if (count == 0)
            {
                return;
            }

            void* mem = m_allocator->Allocate(sizeof(std::atomic<uint16_t>) * count, alignof(std::atomic<uint16_t>));
            m_remaining = static_cast<std::atomic<uint16_t>*>(mem);

            for (size_t i = 0; i < count; ++i)
            {
                new (&m_remaining[i]) std::atomic<uint16_t>{0};
            }
        }

        void ResetRemainingCounts() {
            for (size_t i = 0; i < m_remainingCount; ++i)
            {
                const SystemNode* node = m_graph.GetNode(static_cast<uint32_t>(i));
                if (node != nullptr)
                {
                    m_remaining[i].store(node->DependencyCount(), std::memory_order_relaxed);
                }
            }
        }

        void SubmitSystemTask(uint32_t nodeIndex, World& world, SystemStorage<Allocator>& storage, Tick tick,
                              WaitGroup& wg) {
            // Pack context into task data
            struct TaskData
            {
                ParallelScheduler* m_scheduler;
                World* m_world;
                SystemStorage<Allocator>* m_storage;
                uint32_t m_nodeIndex;
                Tick m_tick;
                WaitGroup* m_wg;
            };

            static constexpr size_t kMaxTasks = 256;
            thread_local TaskData s_tasks[kMaxTasks];
            thread_local size_t s_taskIdx = 0;

            hive::Assert(s_taskIdx < kMaxTasks, "ParallelScheduler: too many concurrent tasks");
            auto& td = s_tasks[s_taskIdx % kMaxTasks];
            s_taskIdx++;

            td.m_scheduler = this;
            td.m_world = &world;
            td.m_storage = &storage;
            td.m_nodeIndex = nodeIndex;
            td.m_tick = tick;
            td.m_wg = &wg;

            m_pool->Submit(
                [](void* data) {
                    auto* td = static_cast<TaskData*>(data);
                    td->m_scheduler->ExecuteSystem(td->m_nodeIndex, *td->m_world, *td->m_storage, td->m_tick,
                                                   *td->m_wg);
                },
                &td);
        }

        void ExecuteSystem(uint32_t nodeIndex, World& world, SystemStorage<Allocator>& storage, Tick tick,
                           WaitGroup& wg) {
            SystemNode* node = m_graph.GetNode(nodeIndex);
            if (node == nullptr)
            {
                wg.Done();
                return;
            }

            node->SetState(SystemState::RUNNING);

            SystemDescriptor<Allocator>* system = storage.GetSystemByIndex(nodeIndex);
            if (system != nullptr)
            {
                HIVE_PROFILE_SCOPE_N("ExecuteSystem");
                HIVE_PROFILE_ZONE_NAME(system->Name(), std::strlen(system->Name()));
                system->Execute(world, tick);
            }

            node->SetState(SystemState::COMPLETE);

            const auto* dependents = m_graph.GetDependents(nodeIndex);
            if (dependents != nullptr)
            {
                for (size_t i = 0; i < dependents->Size(); ++i)
                {
                    uint32_t depIdx = (*dependents)[i];

                    uint16_t prev = m_remaining[depIdx].fetch_sub(1, std::memory_order_acq_rel);

                    // If we were the last dependency, submit the dependent
                    if (prev == 1)
                    {
                        SubmitSystemTask(depIdx, world, storage, tick, wg);
                    }
                }
            }

            wg.Done();
        }

        DependencyGraph<Allocator> m_graph;
        ThreadPool<Allocator>* m_pool;
        bool m_ownsPool;
        std::atomic<uint16_t>* m_remaining; // Remaining dependency count per node
        size_t m_remainingCount;
        Allocator* m_allocator;
    };
} // namespace queen
