#pragma once

#include <queen/scheduler/work_stealing_deque.h>
#include <queen/scheduler/worker_context.h>
#include <comb/allocator_concepts.h>
#include <comb/thread_safe_allocator.h>
#include <atomic>
#include <thread>
#include <mutex>

namespace queen
{
    /**
     * Worker thread state
     */
    enum class WorkerState : uint8_t
    {
        Idle,       // Waiting for work
        Running,    // Executing a task
        Stealing,   // Trying to steal work
        Stopped     // Thread has stopped
    };

    /**
     * Idle strategy for worker threads when no work is available
     */
    enum class IdleStrategy : uint8_t
    {
        Spin,   // Busy-wait (lowest latency, highest CPU usage)
        Yield,  // std::this_thread::yield() (moderate latency/CPU)
        Park    // Condition variable wait (lowest CPU, higher latency)
    };

    /**
     * Task function type
     *
     * Tasks are type-erased function pointers with user data.
     * This avoids heap allocation for std::function.
     */
    struct Task
    {
        using Func = void(*)(void* user_data);

        Func func{nullptr};
        void* user_data{nullptr};

        void Execute() const
        {
            if (func != nullptr)
            {
                func(user_data);
            }
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return func != nullptr;
        }
    };

    /**
     * Worker thread context
     *
     * Contains per-worker state and resources.
     * Each worker has its own deque for work-stealing.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ id: size_t - worker index (0 to N-1)                           │
     * │ state: atomic<WorkerState> - current state                     │
     * │ should_stop: atomic<bool> - shutdown signal                    │
     * │ thread: std::thread - OS thread handle                         │
     * │ deque: WorkStealingDeque* - per-worker task queue              │
     * │ rng_state: uint32_t - random state for victim selection        │
     * └────────────────────────────────────────────────────────────────┘
     */
    template<comb::Allocator Allocator>
    struct WorkerContextT
    {
        // Use ThreadSafeAllocator for thread-safe deque operations
        using SafeAllocator = comb::ThreadSafeAllocator<Allocator>;

        size_t id{0};
        std::atomic<WorkerState> state{WorkerState::Idle};
        std::atomic<bool> should_stop{false};
        std::thread thread;
        WorkStealingDeque<Task, SafeAllocator>* deque{nullptr};
        uint32_t rng_state{0};

        WorkerContextT() = default;
        ~WorkerContextT() = default;

        WorkerContextT(const WorkerContextT&) = delete;
        WorkerContextT& operator=(const WorkerContextT&) = delete;
        WorkerContextT(WorkerContextT&&) = delete;
        WorkerContextT& operator=(WorkerContextT&&) = delete;
    };

    /**
     * Thread pool for parallel task execution
     *
     * Manages a pool of worker threads that can execute tasks in parallel.
     * Workers use work-stealing to balance load across threads.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ workers_: WorkerContextT* - array of worker contexts           │
     * │ worker_count_: size_t - number of workers                      │
     * │ idle_strategy_: IdleStrategy - how to wait when idle           │
     * │ running_: atomic<bool> - pool is running                       │
     * │ pending_tasks_: atomic<int64_t> - outstanding task count       │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Worker count defaults to hardware_concurrency()
     * - Work stealing provides automatic load balancing
     * - Idle strategy trades CPU usage for latency
     * - Tasks are distributed round-robin when submitted
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{10_MB};
     *   queen::ThreadPool<comb::LinearAllocator> pool{alloc};
     *
     *   pool.Start();
     *
     *   // Submit tasks
     *   pool.Submit([](void* data) { DoWork(data); }, nullptr);
     *
     *   // Wait for all tasks
     *   pool.WaitAll();
     *
     *   pool.Stop();
     * @endcode
     */
    template<comb::Allocator Allocator>
    class ThreadPool
    {
    public:
        using WorkerContext = WorkerContextT<Allocator>;
        using SafeAllocator = comb::ThreadSafeAllocator<Allocator>;

        explicit ThreadPool(Allocator& allocator,
                           size_t worker_count = 0,
                           IdleStrategy idle_strategy = IdleStrategy::Yield,
                           size_t deque_capacity = 1024)
            : allocator_{&allocator}
            , safe_allocator_{allocator}  // Thread-safe wrapper
            , workers_{nullptr}
            , global_queue_{nullptr}
            , worker_count_{worker_count == 0 ? GetDefaultWorkerCount() : worker_count}
            , idle_strategy_{idle_strategy}
            , running_{false}
            , pending_tasks_{0}
        {
            // Allocate global submission queue
            // The deque uses safe_allocator_ internally for Grow() which can happen from any thread
            void* global_mem = allocator_->Allocate(
                sizeof(WorkStealingDeque<Task, SafeAllocator>),
                alignof(WorkStealingDeque<Task, SafeAllocator>)
            );
            global_queue_ = new (global_mem) WorkStealingDeque<Task, SafeAllocator>(safe_allocator_, deque_capacity * 4);

            // Allocate worker contexts (single-threaded setup)
            void* mem = allocator_->Allocate(sizeof(WorkerContext) * worker_count_, alignof(WorkerContext));
            workers_ = static_cast<WorkerContext*>(mem);

            // Construct worker contexts
            for (size_t i = 0; i < worker_count_; ++i)
            {
                new (&workers_[i]) WorkerContext();
                workers_[i].id = i;
                workers_[i].rng_state = static_cast<uint32_t>(i + 1);

                // Allocate per-worker deque (single-threaded setup)
                // The deque uses safe_allocator_ internally for Grow() which can happen from workers
                void* deque_mem = allocator_->Allocate(
                    sizeof(WorkStealingDeque<Task, SafeAllocator>),
                    alignof(WorkStealingDeque<Task, SafeAllocator>)
                );
                workers_[i].deque = new (deque_mem) WorkStealingDeque<Task, SafeAllocator>(safe_allocator_, deque_capacity);
            }
        }

        ~ThreadPool()
        {
            Stop();

            // Destroy worker contexts and deques
            for (size_t i = 0; i < worker_count_; ++i)
            {
                if (workers_[i].deque != nullptr)
                {
                    workers_[i].deque->~WorkStealingDeque<Task, SafeAllocator>();
                    allocator_->Deallocate(workers_[i].deque);
                }
                workers_[i].~WorkerContext();
            }

            allocator_->Deallocate(workers_);

            // Destroy global queue
            if (global_queue_ != nullptr)
            {
                global_queue_->~WorkStealingDeque<Task, SafeAllocator>();
                allocator_->Deallocate(global_queue_);
            }
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        /**
         * Start the thread pool
         *
         * Spawns worker threads that begin executing tasks.
         */
        void Start()
        {
            if (running_.exchange(true))
            {
                return; // Already running
            }

            for (size_t i = 0; i < worker_count_; ++i)
            {
                workers_[i].should_stop.store(false, std::memory_order_relaxed);
                workers_[i].thread = std::thread(&ThreadPool::WorkerMain, this, &workers_[i]);
            }
        }

        /**
         * Stop the thread pool
         *
         * Signals all workers to stop and waits for them to finish.
         */
        void Stop()
        {
            if (!running_.exchange(false))
            {
                return; // Already stopped
            }

            // Signal all workers to stop
            for (size_t i = 0; i < worker_count_; ++i)
            {
                workers_[i].should_stop.store(true, std::memory_order_release);
            }

            // Wait for all workers to finish
            for (size_t i = 0; i < worker_count_; ++i)
            {
                if (workers_[i].thread.joinable())
                {
                    workers_[i].thread.join();
                }
            }
        }

        /**
         * Submit a task to the pool
         *
         * The task will be executed by one of the worker threads.
         * Tasks are pushed to a global queue that workers steal from.
         *
         * @param func Function pointer to execute
         * @param user_data User data passed to function
         */
        void Submit(Task::Func func, void* user_data)
        {
            Task task{func, user_data};

            // Increment pending count BEFORE pushing (prevents race with workers)
            pending_tasks_.fetch_add(1, std::memory_order_release);

            // Push to global queue - protected by mutex for multi-producer safety
            {
                std::lock_guard<std::mutex> lock{submit_mutex_};
                global_queue_->Push(task);
            }
        }

        /**
         * Submit a task to the pool (worker hint is currently ignored)
         *
         * Note: Due to Chase-Lev deque constraints, external threads cannot
         * push directly to worker deques. All submissions go through the
         * global queue and workers steal from it.
         *
         * @param worker_idx Target worker index (currently ignored)
         * @param func Function pointer to execute
         * @param user_data User data passed to function
         */
        void SubmitTo([[maybe_unused]] size_t worker_idx, Task::Func func, void* user_data)
        {
            // External threads cannot push to worker deques (Chase-Lev constraint)
            // All submissions go through the global queue
            Submit(func, user_data);
        }

        /**
         * Wait for all submitted tasks to complete
         *
         * Blocks until all tasks have finished executing.
         */
        void WaitAll()
        {
            while (pending_tasks_.load(std::memory_order_acquire) > 0)
            {
                ApplyIdleStrategy();
            }
        }

        /**
         * Check if there are pending tasks
         */
        [[nodiscard]] bool HasPendingTasks() const noexcept
        {
            return pending_tasks_.load(std::memory_order_acquire) > 0;
        }

        /**
         * Get the number of pending tasks
         */
        [[nodiscard]] int64_t PendingTaskCount() const noexcept
        {
            return pending_tasks_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return running_.load(std::memory_order_acquire);
        }

        [[nodiscard]] size_t WorkerCount() const noexcept
        {
            return worker_count_;
        }

        [[nodiscard]] IdleStrategy GetIdleStrategy() const noexcept
        {
            return idle_strategy_;
        }

        [[nodiscard]] WorkerState GetWorkerState(size_t index) const noexcept
        {
            if (index >= worker_count_) return WorkerState::Stopped;
            return workers_[index].state.load(std::memory_order_acquire);
        }

    private:
        static size_t GetDefaultWorkerCount() noexcept
        {
            size_t count = std::thread::hardware_concurrency();
            return count > 0 ? count : 4;
        }

        // Simple xorshift32 for fast random numbers
        static uint32_t XorShift32(uint32_t& state) noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        void WorkerMain(WorkerContext* ctx)
        {
            constexpr int kSpinAttempts = 64; // Spin before yielding
            int idle_spins = 0;

            // Set worker index for this thread (used by World::Query for per-thread allocators)
            queen::WorkerContext::SetCurrentWorkerIndex(ctx->id);

            while (!ctx->should_stop.load(std::memory_order_acquire))
            {
                Task task{};

                // 1. Try to pop from own local deque (for subtasks pushed by self)
                if (auto local_task = ctx->deque->Pop())
                {
                    task = local_task.value();
                    idle_spins = 0;
                }
                // 2. Try to steal from global queue (main submission point)
                else if (auto global_task = global_queue_->Steal())
                {
                    task = global_task.value();
                    idle_spins = 0;
                }
                // 3. Try to steal from other workers
                else
                {
                    ctx->state.store(WorkerState::Stealing, std::memory_order_relaxed);
                    task = TrySteal(ctx);
                    if (task.IsValid())
                    {
                        idle_spins = 0;
                    }
                }

                if (task.IsValid())
                {
                    ctx->state.store(WorkerState::Running, std::memory_order_relaxed);
                    task.Execute();
                    pending_tasks_.fetch_sub(1, std::memory_order_release);
                }
                else
                {
                    ctx->state.store(WorkerState::Idle, std::memory_order_relaxed);
                    ++idle_spins;
                    if (idle_spins >= kSpinAttempts)
                    {
                        ApplyIdleStrategy();
                        idle_spins = 0;
                    }
                    else
                    {
                        // Small pause to reduce contention
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                    }
                }
            }

            // Drain any remaining tasks before stopping
            while (auto local_task = ctx->deque->Pop())
            {
                local_task.value().Execute();
                pending_tasks_.fetch_sub(1, std::memory_order_release);
            }

            // Also drain global queue
            while (auto global_task = global_queue_->Steal())
            {
                global_task.value().Execute();
                pending_tasks_.fetch_sub(1, std::memory_order_release);
            }

            // Clear worker index when thread stops
            queen::WorkerContext::ClearCurrentWorkerIndex();
            ctx->state.store(WorkerState::Stopped, std::memory_order_release);
        }

        Task TrySteal(WorkerContext* ctx)
        {
            // Random starting point to reduce contention
            uint32_t start = XorShift32(ctx->rng_state) % static_cast<uint32_t>(worker_count_);

            for (size_t i = 0; i < worker_count_; ++i)
            {
                size_t victim = (static_cast<size_t>(start) + i) % worker_count_;
                if (victim == ctx->id)
                {
                    continue;
                }

                if (auto stolen = workers_[victim].deque->Steal())
                {
                    return stolen.value();
                }
            }

            return Task{};
        }

        void ApplyIdleStrategy()
        {
            switch (idle_strategy_)
            {
            case IdleStrategy::Spin:
                // Busy-wait: do nothing (compiler barrier to prevent optimization)
                std::atomic_signal_fence(std::memory_order_seq_cst);
                break;

            case IdleStrategy::Yield:
                std::this_thread::yield();
                break;

            case IdleStrategy::Park:
                // TODO: Use condition variable for true parking
                // For now, just yield
                std::this_thread::yield();
                break;
            }
        }

        Allocator* allocator_;
        SafeAllocator safe_allocator_;  // Thread-safe wrapper for deque allocations (Grow)
        WorkerContext* workers_;
        WorkStealingDeque<Task, SafeAllocator>* global_queue_;  // Main submission queue
        mutable std::mutex submit_mutex_;  // Protects global_queue_ Push (multi-producer)
        size_t worker_count_;
        IdleStrategy idle_strategy_;
        std::atomic<bool> running_;
        std::atomic<int64_t> pending_tasks_;
    };
}
