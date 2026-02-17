#pragma once

#include <queen/scheduler/dependency_graph.h>
#include <queen/scheduler/thread_pool.h>
#include <queen/scheduler/parallel.h>
#include <queen/system/system_storage.h>
#include <queen/core/tick.h>
#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>
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
    template<comb::Allocator Allocator>
    class ParallelScheduler
    {
    public:
        /**
         * Create a ParallelScheduler with an existing thread pool
         */
        explicit ParallelScheduler(Allocator& allocator, ThreadPool<Allocator>& pool)
            : graph_{allocator}
            , pool_{&pool}
            , owns_pool_{false}
            , remaining_{nullptr}
            , remaining_count_{0}
            , allocator_{&allocator}
        {}

        /**
         * Create a ParallelScheduler with a new internal thread pool
         */
        explicit ParallelScheduler(Allocator& allocator, size_t worker_count = 0)
            : graph_{allocator}
            , pool_{nullptr}
            , owns_pool_{true}
            , remaining_{nullptr}
            , remaining_count_{0}
            , allocator_{&allocator}
        {
            // Create internal pool
            void* pool_mem = allocator_->Allocate(
                sizeof(ThreadPool<Allocator>),
                alignof(ThreadPool<Allocator>)
            );
            pool_ = new (pool_mem) ThreadPool<Allocator>(allocator, worker_count);
            pool_->Start();
        }

        ~ParallelScheduler()
        {
            // Clean up remaining array
            if (remaining_ != nullptr)
            {
                for (size_t i = 0; i < remaining_count_; ++i)
                {
                    remaining_[i].~atomic<uint16_t>();
                }
                allocator_->Deallocate(remaining_);
            }

            // Clean up internal pool if we own it
            if (owns_pool_ && pool_ != nullptr)
            {
                pool_->Stop();
                pool_->~ThreadPool<Allocator>();
                allocator_->Deallocate(pool_);
            }
        }

        ParallelScheduler(const ParallelScheduler&) = delete;
        ParallelScheduler& operator=(const ParallelScheduler&) = delete;
        ParallelScheduler(ParallelScheduler&&) = delete;
        ParallelScheduler& operator=(ParallelScheduler&&) = delete;

        /**
         * Build/rebuild the dependency graph from system storage
         */
        void Build(const SystemStorage<Allocator>& storage)
        {
            graph_.Build(storage);
            ReallocateRemaining(graph_.NodeCount());
        }

        /**
         * Mark the graph as needing rebuild
         */
        void Invalidate() noexcept
        {
            graph_.MarkDirty();
        }

        /**
         * Check if the graph needs rebuild
         */
        [[nodiscard]] bool NeedsRebuild() const noexcept
        {
            return graph_.IsDirty();
        }

        /**
         * Run all systems in parallel where possible
         *
         * Independent systems are executed concurrently. Dependent systems
         * wait for their dependencies to complete before executing.
         */
        void RunAll(World& world, SystemStorage<Allocator>& storage);  // Implementation in parallel_scheduler_impl.h

        /**
         * Get the dependency graph
         */
        [[nodiscard]] const DependencyGraph<Allocator>& Graph() const noexcept
        {
            return graph_;
        }

        [[nodiscard]] DependencyGraph<Allocator>& Graph() noexcept
        {
            return graph_;
        }

        /**
         * Get the thread pool
         */
        [[nodiscard]] ThreadPool<Allocator>* Pool() noexcept
        {
            return pool_;
        }

        /**
         * Get the execution order (for debugging/visualization)
         */
        [[nodiscard]] const wax::Vector<uint32_t, Allocator>& ExecutionOrder() const noexcept
        {
            return graph_.ExecutionOrder();
        }

        /**
         * Check if the dependency graph has cycles
         */
        [[nodiscard]] bool HasCycle() const noexcept
        {
            return graph_.HasCycle();
        }

    private:
        void ReallocateRemaining(size_t count)
        {
            // Clean up old array
            if (remaining_ != nullptr)
            {
                for (size_t i = 0; i < remaining_count_; ++i)
                {
                    remaining_[i].~atomic<uint16_t>();
                }
                allocator_->Deallocate(remaining_);
                remaining_ = nullptr;
            }

            remaining_count_ = count;
            if (count == 0)
            {
                return;
            }

            // Allocate new array
            void* mem = allocator_->Allocate(
                sizeof(std::atomic<uint16_t>) * count,
                alignof(std::atomic<uint16_t>)
            );
            remaining_ = static_cast<std::atomic<uint16_t>*>(mem);

            // Construct atomics
            for (size_t i = 0; i < count; ++i)
            {
                new (&remaining_[i]) std::atomic<uint16_t>{0};
            }
        }

        void ResetRemainingCounts()
        {
            for (size_t i = 0; i < remaining_count_; ++i)
            {
                const SystemNode* node = graph_.GetNode(static_cast<uint32_t>(i));
                if (node != nullptr)
                {
                    remaining_[i].store(node->DependencyCount(), std::memory_order_relaxed);
                }
            }
        }

        void SubmitSystemTask(
            uint32_t node_index,
            World& world,
            SystemStorage<Allocator>& storage,
            Tick tick,
            WaitGroup& wg)
        {
            // Pack context into task data
            struct TaskData
            {
                ParallelScheduler* scheduler;
                World* world;
                SystemStorage<Allocator>* storage;
                uint32_t node_index;
                Tick tick;
                WaitGroup* wg;
            };

            static constexpr size_t kMaxTasks = 256;
            thread_local TaskData tasks[kMaxTasks];
            thread_local size_t task_idx = 0;

            hive::Assert(task_idx < kMaxTasks, "ParallelScheduler: too many concurrent tasks");
            auto& td = tasks[task_idx % kMaxTasks];
            task_idx++;

            td.scheduler = this;
            td.world = &world;
            td.storage = &storage;
            td.node_index = node_index;
            td.tick = tick;
            td.wg = &wg;

            pool_->Submit([](void* data) {
                auto* td = static_cast<TaskData*>(data);
                td->scheduler->ExecuteSystem(
                    td->node_index,
                    *td->world,
                    *td->storage,
                    td->tick,
                    *td->wg
                );
            }, &td);
        }

        void ExecuteSystem(
            uint32_t node_index,
            World& world,
            SystemStorage<Allocator>& storage,
            Tick tick,
            WaitGroup& wg)
        {
            SystemNode* node = graph_.GetNode(node_index);
            if (node == nullptr)
            {
                wg.Done();
                return;
            }

            // Mark as running
            node->SetState(SystemState::Running);

            // Execute the system
            SystemDescriptor<Allocator>* system = storage.GetSystemByIndex(node_index);
            if (system != nullptr)
            {
                HIVE_PROFILE_SCOPE_N("ExecuteSystem");
                HIVE_PROFILE_ZONE_NAME(system->Name(), std::strlen(system->Name()));
                system->Execute(world, tick);
            }

            // Mark as complete
            node->SetState(SystemState::Complete);

            // Notify dependents
            const auto* dependents = graph_.GetDependents(node_index);
            if (dependents != nullptr)
            {
                for (size_t i = 0; i < dependents->Size(); ++i)
                {
                    uint32_t dep_idx = (*dependents)[i];

                    // Decrement remaining count atomically
                    uint16_t prev = remaining_[dep_idx].fetch_sub(1, std::memory_order_acq_rel);

                    // If we were the last dependency, submit the dependent
                    if (prev == 1)
                    {
                        SubmitSystemTask(dep_idx, world, storage, tick, wg);
                    }
                }
            }

            // Signal completion
            wg.Done();
        }

        DependencyGraph<Allocator> graph_;
        ThreadPool<Allocator>* pool_;
        bool owns_pool_;
        std::atomic<uint16_t>* remaining_;  // Remaining dependency count per node
        size_t remaining_count_;
        Allocator* allocator_;
    };
}
