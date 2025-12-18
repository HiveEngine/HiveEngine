#pragma once

#include <cstddef>
#include <cstdint>

namespace queen
{
    /**
     * Thread-local worker context for parallel execution
     *
     * This provides a way for code executing in worker threads to know
     * which worker they're running on, enabling per-worker resource access
     * (like thread-local allocators).
     *
     * Usage:
     * - ThreadPool sets the worker index when a task starts
     * - Systems can query the current worker index
     * - World::Query() uses this to select per-thread allocators
     *
     * Special values:
     * - kMainThread (SIZE_MAX): Running on main thread (not in parallel)
     * - 0 to N-1: Running on worker thread with that index
     */
    struct WorkerContext
    {
        static constexpr size_t kMainThread = SIZE_MAX;

        /**
         * Get current worker index
         *
         * Returns kMainThread if called from main thread (not in parallel),
         * otherwise returns the worker index (0 to N-1).
         */
        [[nodiscard]] static size_t CurrentWorkerIndex() noexcept
        {
            return t_current_worker_index;
        }

        /**
         * Check if currently executing in a parallel worker
         */
        [[nodiscard]] static bool IsInWorker() noexcept
        {
            return t_current_worker_index != kMainThread;
        }

        /**
         * Set current worker index (called by ThreadPool)
         *
         * @param index Worker index (0 to N-1)
         */
        static void SetCurrentWorkerIndex(size_t index) noexcept
        {
            t_current_worker_index = index;
        }

        /**
         * Clear current worker index (called when task completes)
         */
        static void ClearCurrentWorkerIndex() noexcept
        {
            t_current_worker_index = kMainThread;
        }

    private:
        // Thread-local storage for current worker index
        inline static thread_local size_t t_current_worker_index = kMainThread;
    };
}
