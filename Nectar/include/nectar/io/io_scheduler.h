#pragma once

#include <nectar/io/io_request.h>
#include <wax/containers/vector.h>
#include <wax/containers/hash_set.h>
#include <comb/default_allocator.h>
#include <hive/profiling/profiler.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstddef>

namespace nectar
{
    class VirtualFilesystem;

    struct IOSchedulerConfig
    {
        size_t worker_count{2};
    };

    class IOScheduler
    {
    public:
        IOScheduler(VirtualFilesystem& vfs, comb::DefaultAllocator& alloc,
                    IOSchedulerConfig config = {});
        ~IOScheduler();

        IOScheduler(const IOScheduler&) = delete;
        IOScheduler& operator=(const IOScheduler&) = delete;

        /// Submit a read request. Returns the request ID. Thread-safe.
        [[nodiscard]] IORequestId Submit(wax::StringView path, LoadPriority priority);

        /// Cancel a pending request. If already in-flight, result is discarded.
        void Cancel(IORequestId id);

        /// Drain completed requests into `out`. Called from main thread.
        /// Returns number of completions drained.
        size_t DrainCompletions(wax::Vector<IOCompletion>& out);

        /// Shut down workers (blocks until all threads join).
        void Shutdown();

        [[nodiscard]] size_t PendingCount() const;
        [[nodiscard]] bool IsShutdown() const noexcept;

    private:
        void WorkerLoop();

        VirtualFilesystem* vfs_;
        comb::DefaultAllocator* alloc_;

        // request_mutex_ NOT lockable-tracked — used with condition_variable (incompatible)
        mutable std::mutex request_mutex_;
        std::condition_variable request_cv_;
        wax::Vector<IORequest> request_queue_;

        HIVE_PROFILE_LOCKABLE_N(std::mutex, completion_mutex_, "IO.CompletionMutex");
        wax::Vector<IOCompletion> completion_queue_;

        // Tracks requests cancelled while in-flight
        wax::HashSet<IORequestId> cancelled_ids_;

        std::vector<std::thread> workers_;

        std::atomic<bool> shutdown_{false};
        std::atomic<int> worker_id_counter_{0};
        IORequestId next_id_{0};
    };
}
