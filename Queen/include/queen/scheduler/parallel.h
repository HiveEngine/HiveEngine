#pragma once

#include <queen/scheduler/thread_pool.h>
#include <atomic>
#include <cstddef>

namespace queen
{
    /**
     * WaitGroup for synchronizing parallel tasks
     *
     * Allows waiting for a group of tasks to complete.
     * Similar to Go's sync.WaitGroup.
     *
     * Thread-safe: Yes (atomic counter)
     *
     * Example:
     * @code
     *   WaitGroup wg;
     *   wg.Add(3);
     *
     *   pool.Submit([](void* data) {
     *       // Do work...
     *       static_cast<WaitGroup*>(data)->Done();
     *   }, &wg);
     *   // ... submit 2 more tasks
     *
     *   wg.Wait();  // Blocks until all 3 tasks call Done()
     * @endcode
     */
    class WaitGroup
    {
    public:
        WaitGroup() : counter_{0} {}

        /**
         * Add to the counter (call before submitting tasks)
         */
        void Add(int64_t delta = 1) noexcept
        {
            counter_.fetch_add(delta, std::memory_order_release);
        }

        /**
         * Decrement the counter (call when task completes)
         */
        void Done() noexcept
        {
            counter_.fetch_sub(1, std::memory_order_release);
        }

        /**
         * Wait for counter to reach zero
         */
        void Wait() const noexcept
        {
            while (counter_.load(std::memory_order_acquire) > 0)
            {
                std::this_thread::yield();
            }
        }

        /**
         * Check if all tasks are done without blocking
         */
        [[nodiscard]] bool IsDone() const noexcept
        {
            return counter_.load(std::memory_order_acquire) <= 0;
        }

        /**
         * Get the current count
         */
        [[nodiscard]] int64_t Count() const noexcept
        {
            return counter_.load(std::memory_order_acquire);
        }

    private:
        std::atomic<int64_t> counter_;
    };

    /**
     * Context for parallel_for iteration
     */
    struct ParallelForContext
    {
        using Func = void(*)(size_t index, void* user_data);

        Func func;
        void* user_data;
        size_t index;
        WaitGroup* wait_group;
    };

    /**
     * Execute a function in parallel over a range [begin, end)
     *
     * Divides the range into chunks and executes them across worker threads.
     * Blocks until all iterations complete.
     *
     * @tparam Allocator Allocator type for the thread pool
     * @param pool Thread pool to use for execution
     * @param begin Start index (inclusive)
     * @param end End index (exclusive)
     * @param func Function to call for each index: void(size_t index, void* user_data)
     * @param user_data User data passed to each invocation
     * @param chunk_size Number of iterations per task (0 = auto)
     *
     * Example:
     * @code
     *   std::vector<int> data(1000);
     *
     *   parallel_for(pool, 0, data.size(),
     *       [](size_t i, void* ud) {
     *           auto* vec = static_cast<std::vector<int>*>(ud);
     *           (*vec)[i] = static_cast<int>(i * 2);
     *       }, &data);
     * @endcode
     */
    template<comb::Allocator Allocator>
    void parallel_for(
        ThreadPool<Allocator>& pool,
        size_t begin,
        size_t end,
        void(*func)(size_t index, void* user_data),
        void* user_data,
        size_t chunk_size = 0)
    {
        if (begin >= end)
        {
            return;
        }

        const size_t total = end - begin;

        // Auto chunk size: divide work among workers
        if (chunk_size == 0)
        {
            size_t workers = pool.WorkerCount();
            chunk_size = (total + workers - 1) / workers;
            if (chunk_size == 0) chunk_size = 1;
        }

        // Calculate number of chunks
        const size_t num_chunks = (total + chunk_size - 1) / chunk_size;

        WaitGroup wg;
        wg.Add(static_cast<int64_t>(num_chunks));

        // Submit chunk tasks
        for (size_t chunk = 0; chunk < num_chunks; ++chunk)
        {
            const size_t chunk_begin = begin + chunk * chunk_size;
            const size_t chunk_end = (chunk_begin + chunk_size < end) ? (chunk_begin + chunk_size) : end;

            // Pack chunk info into task
            struct ChunkData
            {
                void(*func)(size_t, void*);
                void* user_data;
                size_t begin;
                size_t end;
                WaitGroup* wg;
            };

            // We need to allocate ChunkData somewhere - use a static approach for simplicity
            // In production, this would use an allocator
            thread_local ChunkData chunks[1024];
            thread_local size_t chunk_idx = 0;

            auto& cd = chunks[chunk_idx % 1024];
            chunk_idx++;

            cd.func = func;
            cd.user_data = user_data;
            cd.begin = chunk_begin;
            cd.end = chunk_end;
            cd.wg = &wg;

            pool.Submit([](void* data) {
                auto* cd = static_cast<ChunkData*>(data);
                for (size_t i = cd->begin; i < cd->end; ++i)
                {
                    cd->func(i, cd->user_data);
                }
                cd->wg->Done();
            }, &cd);
        }

        wg.Wait();
    }

    /**
     * Execute a function in parallel for each element in a range
     *
     * Simplified version that processes one element per task.
     * Use parallel_for with chunk_size for better performance on large ranges.
     *
     * @tparam Allocator Allocator type
     * @param pool Thread pool
     * @param begin Start index
     * @param end End index
     * @param func Function to call
     * @param user_data User data
     */
    template<comb::Allocator Allocator>
    void parallel_for_each(
        ThreadPool<Allocator>& pool,
        size_t begin,
        size_t end,
        void(*func)(size_t index, void* user_data),
        void* user_data)
    {
        parallel_for(pool, begin, end, func, user_data, 1);
    }

    /**
     * Batch context for submitting multiple related tasks
     *
     * Provides a way to submit multiple tasks and wait for all of them.
     *
     * Example:
     * @code
     *   TaskBatch batch;
     *
     *   batch.Submit(pool, task1_func, task1_data);
     *   batch.Submit(pool, task2_func, task2_data);
     *   batch.Submit(pool, task3_func, task3_data);
     *
     *   batch.Wait();  // Blocks until all tasks complete
     * @endcode
     */
    class TaskBatch
    {
    public:
        TaskBatch() = default;

        /**
         * Submit a task to the batch
         */
        template<comb::Allocator Allocator>
        void Submit(ThreadPool<Allocator>& pool, Task::Func func, void* user_data)
        {
            wg_.Add(1);

            // Wrap the task to call Done() on completion
            struct WrappedTask
            {
                Task::Func func;
                void* user_data;
                WaitGroup* wg;
            };

            thread_local WrappedTask wrapped_tasks[1024];
            thread_local size_t wrapped_idx = 0;

            auto& wt = wrapped_tasks[wrapped_idx % 1024];
            wrapped_idx++;

            wt.func = func;
            wt.user_data = user_data;
            wt.wg = &wg_;

            pool.Submit([](void* data) {
                auto* wt = static_cast<WrappedTask*>(data);
                wt->func(wt->user_data);
                wt->wg->Done();
            }, &wt);
        }

        /**
         * Wait for all submitted tasks to complete
         */
        void Wait()
        {
            wg_.Wait();
        }

        /**
         * Check if all tasks are done
         */
        [[nodiscard]] bool IsDone() const noexcept
        {
            return wg_.IsDone();
        }

        /**
         * Get the number of pending tasks
         */
        [[nodiscard]] int64_t PendingCount() const noexcept
        {
            return wg_.Count();
        }

    private:
        WaitGroup wg_;
    };
}
