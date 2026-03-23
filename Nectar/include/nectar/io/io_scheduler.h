#pragma once

#include <hive/profiling/profiler.h>

#include <comb/default_allocator.h>

#include <wax/containers/hash_set.h>
#include <wax/containers/vector.h>

#include <drone/job_submitter.h>
#include <drone/task.h>

#include <nectar/io/io_request.h>

#include <cstddef>
#include <mutex>

namespace nectar
{
    class VirtualFilesystem;

    class HIVE_API IOScheduler
    {
    public:
        IOScheduler(VirtualFilesystem& vfs, comb::DefaultAllocator& alloc, drone::JobSubmitter jobs);
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

        /// Coroutine-based async read. Schedules IO on a worker and suspends
        /// the caller until the data is ready. No completion queue involved.
        [[nodiscard]] drone::Task<wax::ByteBuffer> ReadAsync(wax::StringView path);

        void Shutdown();
        [[nodiscard]] bool IsShutdown() const noexcept;
        [[nodiscard]] size_t PendingCount() const;

    private:
        struct IOJobData
        {
            IOScheduler* m_self;
            IORequestId m_id;
            wax::String m_path;
        };

        static void ExecuteIOJob(void* data);

        VirtualFilesystem* m_vfs;
        comb::DefaultAllocator* m_alloc;
        drone::JobSubmitter m_jobs;

        HIVE_PROFILE_LOCKABLE_N(std::mutex, m_completionMutex, "IO.CompletionMutex");
        wax::Vector<IOCompletion> m_completionQueue;

        mutable std::mutex m_cancelMutex;
        wax::HashSet<IORequestId> m_cancelledIds;

        std::atomic<bool> m_shutdown{false};
        std::atomic<size_t> m_pendingCount{0};
        std::atomic<IORequestId> m_nextId{0};
    };
} // namespace nectar
