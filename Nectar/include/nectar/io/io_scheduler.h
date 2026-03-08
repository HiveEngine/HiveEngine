#pragma once

#include <hive/profiling/profiler.h>

#include <comb/default_allocator.h>

#include <wax/containers/hash_set.h>
#include <wax/containers/vector.h>

#include <nectar/io/io_request.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

namespace nectar
{
    class VirtualFilesystem;

    struct IOSchedulerConfig
    {
        size_t m_workerCount{2};
    };

    class IOScheduler
    {
    public:
        IOScheduler(VirtualFilesystem& vfs, comb::DefaultAllocator& alloc, IOSchedulerConfig config = {});
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

        VirtualFilesystem* m_vfs;
        comb::DefaultAllocator* m_alloc;

        // request_mutex_ NOT lockable-tracked — used with condition_variable (incompatible)
        mutable std::mutex m_requestMutex;
        std::condition_variable m_requestCv;
        wax::Vector<IORequest> m_requestQueue;

        HIVE_PROFILE_LOCKABLE_N(std::mutex, m_completionMutex, "IO.CompletionMutex");
        wax::Vector<IOCompletion> m_completionQueue;

        // Tracks requests cancelled while in-flight
        wax::HashSet<IORequestId> m_cancelledIds;

        std::vector<std::thread> m_workers;

        std::atomic<bool> m_shutdown{false};
        std::atomic<int> m_workerIdCounter{0};
        IORequestId m_nextId{0};
    };
} // namespace nectar
