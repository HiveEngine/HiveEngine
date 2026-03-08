#pragma once

#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>
#include <comb/thread_safe_allocator.h>

#include <queen/scheduler/work_stealing_deque.h>
#include <queen/scheduler/worker_context.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace queen
{
    /**
     * Worker thread state
     */
    enum class WorkerState : uint8_t
    {
        IDLE,     // Waiting for work
        RUNNING,  // Executing a task
        STEALING, // Trying to steal work
        STOPPED   // Thread has stopped
    };

    /**
     * Idle strategy for worker threads when no work is available
     */
    enum class IdleStrategy : uint8_t
    {
        SPIN,  // Busy-wait (lowest latency, highest CPU usage)
        YIELD, // std::this_thread::yield() (moderate latency/CPU)
        PARK   // Condition variable wait (lowest CPU, higher latency)
    };

    /**
     * Task function type
     *
     * Tasks are type-erased function pointers with user data.
     * This avoids heap allocation for std::function.
     */
    struct Task
    {
        using Func = void (*)(void* userData);

        Func m_func{nullptr};
        void* m_userData{nullptr};

        void Execute() const {
            if (m_func != nullptr)
            {
                m_func(m_userData);
            }
        }

        [[nodiscard]] bool IsValid() const noexcept { return m_func != nullptr; }
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
    template <comb::Allocator Allocator> struct WorkerContextT
    {
        // Use ThreadSafeAllocator for thread-safe deque operations
        using SafeAllocator = comb::ThreadSafeAllocator<Allocator>;

        size_t m_id{0};
        std::atomic<WorkerState> m_state{WorkerState::IDLE};
        std::atomic<bool> m_shouldStop{false};
        std::thread m_thread;
        WorkStealingDeque<Task, SafeAllocator>* m_deque{nullptr};
        uint32_t m_rngState{0};

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
    template <comb::Allocator Allocator> class ThreadPool
    {
    public:
        using WorkerContext = WorkerContextT<Allocator>;
        using SafeAllocator = comb::ThreadSafeAllocator<Allocator>;

        explicit ThreadPool(Allocator& allocator, size_t workerCount = 0,
                            IdleStrategy idleStrategy = IdleStrategy::YIELD, size_t dequeCapacity = 1024)
            : m_allocator{&allocator}
            , m_safeAllocator{allocator} // Thread-safe wrapper
            , m_workers{nullptr}
            , m_globalQueue{nullptr}
            , m_workerCount{workerCount == 0 ? GetDefaultWorkerCount() : workerCount}
            , m_idleStrategy{idleStrategy}
            , m_running{false}
            , m_pendingTasks{0} {
            // The deque uses safe_allocator_ internally for Grow() which can happen from any thread
            void* globalMem = m_allocator->Allocate(sizeof(WorkStealingDeque<Task, SafeAllocator>),
                                                    alignof(WorkStealingDeque<Task, SafeAllocator>));
            m_globalQueue = new (globalMem) WorkStealingDeque<Task, SafeAllocator>{m_safeAllocator, dequeCapacity * 4};

            // Single-threaded setup phase
            void* mem = m_allocator->Allocate(sizeof(WorkerContext) * m_workerCount, alignof(WorkerContext));
            m_workers = static_cast<WorkerContext*>(mem);

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                new (&m_workers[i]) WorkerContext{};
                m_workers[i].m_id = i;
                m_workers[i].m_rngState = static_cast<uint32_t>(i + 1);

                // The deque uses safe_allocator_ internally for Grow() which can happen from workers
                void* dequeMem = m_allocator->Allocate(sizeof(WorkStealingDeque<Task, SafeAllocator>),
                                                       alignof(WorkStealingDeque<Task, SafeAllocator>));
                m_workers[i].m_deque =
                    new (dequeMem) WorkStealingDeque<Task, SafeAllocator>{m_safeAllocator, dequeCapacity};
            }
        }

        ~ThreadPool() {
            Stop();

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                if (m_workers[i].m_deque != nullptr)
                {
                    m_workers[i].m_deque->~WorkStealingDeque<Task, SafeAllocator>();
                    m_allocator->Deallocate(m_workers[i].m_deque);
                }
                m_workers[i].~WorkerContext();
            }

            m_allocator->Deallocate(m_workers);

            if (m_globalQueue != nullptr)
            {
                m_globalQueue->~WorkStealingDeque<Task, SafeAllocator>();
                m_allocator->Deallocate(m_globalQueue);
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
        void Start() {
            if (m_running.exchange(true))
            {
                return; // Already running
            }

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                m_workers[i].m_shouldStop.store(false, std::memory_order_relaxed);
                m_workers[i].m_thread = std::thread(&ThreadPool::WorkerMain, this, &m_workers[i]);
            }
        }

        /**
         * Stop the thread pool
         *
         * Signals all workers to stop and waits for them to finish.
         */
        void Stop() {
            if (!m_running.exchange(false))
            {
                return; // Already stopped
            }

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                m_workers[i].m_shouldStop.store(true, std::memory_order_release);
            }

            // Wake all parked workers so they see should_stop
            m_parkCv.notify_all();

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                if (m_workers[i].m_thread.joinable())
                {
                    m_workers[i].m_thread.join();
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
        void Submit(Task::Func func, void* userData) {
            HIVE_PROFILE_SCOPE_N("ThreadPool::Submit");

            Task task{func, userData};

            // Increment pending count BEFORE pushing (prevents race with workers)
            m_pendingTasks.fetch_add(1, std::memory_order_release);

            // Push to global queue - protected by mutex for multi-producer safety
            {
                std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_submitMutex};
                m_globalQueue->Push(task);
            }

            // Wake a parked worker if using Park strategy
            if (m_idleStrategy == IdleStrategy::PARK)
            {
                m_parkCv.notify_one();
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
        void SubmitTo([[maybe_unused]] size_t workerIdx, Task::Func func, void* userData) {
            // External threads cannot push to worker deques (Chase-Lev constraint)
            // All submissions go through the global queue
            Submit(func, userData);
        }

        /**
         * Wait for all submitted tasks to complete
         *
         * Blocks until all tasks have finished executing.
         */
        void WaitAll() {
            while (m_pendingTasks.load(std::memory_order_acquire) > 0)
            {
                ApplyIdleStrategy();
            }
        }

        /**
         * Check if there are pending tasks
         */
        [[nodiscard]] bool HasPendingTasks() const noexcept {
            return m_pendingTasks.load(std::memory_order_acquire) > 0;
        }

        /**
         * Get the number of pending tasks
         */
        [[nodiscard]] int64_t PendingTaskCount() const noexcept {
            return m_pendingTasks.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool IsRunning() const noexcept { return m_running.load(std::memory_order_acquire); }

        [[nodiscard]] size_t WorkerCount() const noexcept { return m_workerCount; }

        [[nodiscard]] IdleStrategy GetIdleStrategy() const noexcept { return m_idleStrategy; }

        [[nodiscard]] WorkerState GetWorkerState(size_t index) const noexcept {
            if (index >= m_workerCount)
                return WorkerState::STOPPED;
            return m_workers[index].m_state.load(std::memory_order_acquire);
        }

    private:
        static size_t GetDefaultWorkerCount() noexcept {
            size_t count = std::thread::hardware_concurrency();
            return count > 0 ? count : 4;
        }

        // Simple xorshift32 for fast random numbers
        static uint32_t XorShift32(uint32_t& state) noexcept {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        void WorkerMain(WorkerContext* ctx) {
            constexpr int kSpinAttempts = 64; // Spin before yielding
            int idleSpins = 0;

            // Name this thread for Tracy
            char threadName[32];
            std::snprintf(threadName, sizeof(threadName), "Worker %zu", ctx->m_id);
            HIVE_PROFILE_THREAD(threadName);

            // Set worker index for this thread (used by World::Query for per-thread allocators)
            queen::WorkerContext::SetCurrentWorkerIndex(ctx->m_id);

            while (!ctx->m_shouldStop.load(std::memory_order_acquire))
            {
                Task task{};

                // 1. Try to pop from own local deque (for subtasks pushed by self)
                if (auto localTask = ctx->m_deque->Pop())
                {
                    task = localTask.value();
                    idleSpins = 0;
                }
                // 2. Try to steal from global queue (main submission point)
                else if (auto globalTask = m_globalQueue->Steal())
                {
                    task = globalTask.value();
                    idleSpins = 0;
                }
                // 3. Try to steal from other workers
                else
                {
                    ctx->m_state.store(WorkerState::STEALING, std::memory_order_relaxed);
                    task = TrySteal(ctx);
                    if (task.IsValid())
                    {
                        idleSpins = 0;
                    }
                }

                if (task.IsValid())
                {
                    ctx->m_state.store(WorkerState::RUNNING, std::memory_order_relaxed);
                    {
                        HIVE_PROFILE_SCOPE_N("TaskExecute");
                        task.Execute();
                    }
                    m_pendingTasks.fetch_sub(1, std::memory_order_release);
                }
                else
                {
                    ctx->m_state.store(WorkerState::IDLE, std::memory_order_relaxed);
                    ++idleSpins;
                    if (idleSpins >= kSpinAttempts)
                    {
                        ApplyIdleStrategy();
                        idleSpins = 0;
                    }
                    else
                    {
                        // Small pause to reduce contention
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                    }
                }
            }

            // Drain any remaining tasks before stopping
            while (auto localTask = ctx->m_deque->Pop())
            {
                localTask.value().Execute();
                m_pendingTasks.fetch_sub(1, std::memory_order_release);
            }

            // Also drain global queue
            while (auto globalTask = m_globalQueue->Steal())
            {
                globalTask.value().Execute();
                m_pendingTasks.fetch_sub(1, std::memory_order_release);
            }

            // Clear worker index when thread stops
            queen::WorkerContext::ClearCurrentWorkerIndex();
            ctx->m_state.store(WorkerState::STOPPED, std::memory_order_release);
        }

        Task TrySteal(WorkerContext* ctx) {
            HIVE_PROFILE_SCOPE_N("TrySteal");
            // Random starting point to reduce contention
            uint32_t start = XorShift32(ctx->m_rngState) % static_cast<uint32_t>(m_workerCount);

            for (size_t i = 0; i < m_workerCount; ++i)
            {
                size_t victim = (static_cast<size_t>(start) + i) % m_workerCount;
                if (victim == ctx->m_id)
                {
                    continue;
                }

                if (auto stolen = m_workers[victim].m_deque->Steal())
                {
                    return stolen.value();
                }
            }

            return Task{};
        }

        void ApplyIdleStrategy() {
            switch (m_idleStrategy)
            {
                case IdleStrategy::SPIN:
                    // Busy-wait: do nothing (compiler barrier to prevent optimization)
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    break;

                case IdleStrategy::YIELD:
                    std::this_thread::yield();
                    break;

                case IdleStrategy::PARK: {
                    std::unique_lock<std::mutex> lock{m_parkMutex};
                    m_parkCv.wait_for(lock, std::chrono::milliseconds(1), [this] {
                        return m_pendingTasks.load(std::memory_order_acquire) > 0 ||
                               !m_running.load(std::memory_order_acquire);
                    });
                    break;
                }
            }
        }

        Allocator* m_allocator;
        SafeAllocator m_safeAllocator; // Thread-safe wrapper for deque allocations (Grow)
        WorkerContext* m_workers;
        WorkStealingDeque<Task, SafeAllocator>* m_globalQueue; // Main submission queue
        mutable HIVE_PROFILE_LOCKABLE_N(std::mutex, m_submitMutex, "SubmitMutex");
        std::condition_variable m_parkCv; // Wakes parked workers
        std::mutex m_parkMutex;           // Protects park_cv_ (not tracked, used with condition_variable)
        size_t m_workerCount;
        IdleStrategy m_idleStrategy;
        std::atomic<bool> m_running;
        std::atomic<int64_t> m_pendingTasks;
    };
} // namespace queen
