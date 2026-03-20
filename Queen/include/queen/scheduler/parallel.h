#pragma once

#include <hive/core/assert.h>

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
        WaitGroup()
            : m_counter{0}
        {
        }

        /**
         * Add to the counter (call before submitting tasks)
         */
        void Add(int64_t delta = 1) noexcept
        {
            m_counter.fetch_add(delta, std::memory_order_release);
        }

        /**
         * Decrement the counter (call when task completes)
         */
        void Done() noexcept
        {
            if (m_counter.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                m_counter.notify_all();
            }
        }

        /**
         * Wait for counter to reach zero
         */
        void Wait() const noexcept
        {
            for (;;)
            {
                int64_t val = m_counter.load(std::memory_order_acquire);
                if (val <= 0)
                    return;
                m_counter.wait(val, std::memory_order_relaxed);
            }
        }

        /**
         * Check if all tasks are done without blocking
         */
        [[nodiscard]] bool IsDone() const noexcept
        {
            return m_counter.load(std::memory_order_acquire) <= 0;
        }

        /**
         * Get the current count
         */
        [[nodiscard]] int64_t Count() const noexcept
        {
            return m_counter.load(std::memory_order_acquire);
        }

    private:
        std::atomic<int64_t> m_counter;
    };

    /**
     * Context for parallel_for iteration
     */
    struct ParallelForContext
    {
        using Func = void (*)(size_t index, void* userData);

        Func m_func;
        void* m_userData;
        size_t m_index;
        WaitGroup* m_waitGroup;
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
     *   wax::Vector<int, Alloc> data{alloc};
     *   data.Resize(1000);
     *
     *   parallel_for(pool, 0, data.Size(),
     *       [](size_t i, void* ud) {
     *           auto* vec = static_cast<wax::Vector<int, Alloc>*>(ud);
     *           (*vec)[i] = static_cast<int>(i * 2);
     *       }, &data);
     * @endcode
     */
    template <comb::Allocator Allocator>
    void ParallelFor(ThreadPool<Allocator>& pool, size_t begin, size_t end, void (*func)(size_t index, void* userData),
                     void* userData, size_t chunkSize = 0)
    {
        if (begin >= end)
        {
            return;
        }

        const size_t total = end - begin;

        // Auto chunk size: divide work among workers
        if (chunkSize == 0)
        {
            size_t workers = pool.WorkerCount();
            chunkSize = (total + workers - 1) / workers;
            if (chunkSize == 0)
                chunkSize = 1;
        }

        // Calculate number of chunks
        const size_t numChunks = (total + chunkSize - 1) / chunkSize;

        WaitGroup wg;
        wg.Add(static_cast<int64_t>(numChunks));

        // Submit chunk tasks
        for (size_t chunk = 0; chunk < numChunks; ++chunk)
        {
            const size_t chunkBegin = begin + chunk * chunkSize;
            const size_t chunkEnd = (chunkBegin + chunkSize < end) ? (chunkBegin + chunkSize) : end;

            // Pack chunk info into task
            struct ChunkData
            {
                void (*m_func)(size_t, void*);
                void* m_userData;
                size_t m_begin;
                size_t m_end;
                WaitGroup* m_wg;
            };

            static constexpr size_t kMaxChunks = 1024;
            thread_local ChunkData s_chunks[kMaxChunks];
            thread_local size_t s_chunkIdx = 0;

            hive::Assert(numChunks <= kMaxChunks, "parallel_for: too many chunks, increase kMaxChunks or chunk_size");
            auto& cd = s_chunks[s_chunkIdx % kMaxChunks];
            s_chunkIdx++;

            cd.m_func = func;
            cd.m_userData = userData;
            cd.m_begin = chunkBegin;
            cd.m_end = chunkEnd;
            cd.m_wg = &wg;

            pool.Submit(
                [](void* data) {
                    auto* cd = static_cast<ChunkData*>(data);
                    for (size_t i = cd->m_begin; i < cd->m_end; ++i)
                    {
                        cd->m_func(i, cd->m_userData);
                    }
                    cd->m_wg->Done();
                },
                &cd);
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
    template <comb::Allocator Allocator>
    void ParallelForEach(ThreadPool<Allocator>& pool, size_t begin, size_t end,
                         void (*func)(size_t index, void* userData), void* userData)
    {
        ParallelFor(pool, begin, end, func, userData, 1);
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
        template <comb::Allocator Allocator> void Submit(ThreadPool<Allocator>& pool, Task::Func func, void* userData)
        {
            m_wg.Add(1);

            // Wrap the task to call Done() on completion
            struct WrappedTask
            {
                Task::Func m_func;
                void* m_userData;
                WaitGroup* m_wg;
            };

            static constexpr size_t kMaxWrapped = 1024;
            thread_local WrappedTask s_wrappedTasks[kMaxWrapped];
            thread_local size_t s_wrappedIdx = 0;

            hive::Assert(m_wg.Count() < static_cast<int64_t>(kMaxWrapped),
                         "TaskBatch: too many pending tasks, call Wait() before submitting more");
            auto& wt = s_wrappedTasks[s_wrappedIdx++ % kMaxWrapped];

            wt.m_func = func;
            wt.m_userData = userData;
            wt.m_wg = &m_wg;

            pool.Submit(
                [](void* data) {
                    auto* wt = static_cast<WrappedTask*>(data);
                    wt->m_func(wt->m_userData);
                    wt->m_wg->Done();
                },
                &wt);
        }

        /**
         * Wait for all submitted tasks to complete
         */
        void Wait()
        {
            m_wg.Wait();
        }

        /**
         * Check if all tasks are done
         */
        [[nodiscard]] bool IsDone() const noexcept
        {
            return m_wg.IsDone();
        }

        /**
         * Get the number of pending tasks
         */
        [[nodiscard]] int64_t PendingCount() const noexcept
        {
            return m_wg.Count();
        }

    private:
        WaitGroup m_wg;
    };
} // namespace queen
