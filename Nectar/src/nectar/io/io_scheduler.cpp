#include <drone/schedule_on.h>

#include <nectar/io/io_scheduler.h>
#include <nectar/vfs/virtual_filesystem.h>

#include <thread>

namespace nectar
{
    IOScheduler::IOScheduler(VirtualFilesystem& vfs, comb::DefaultAllocator& alloc, drone::JobSubmitter jobs)
        : m_vfs{&vfs}
        , m_alloc{&alloc}
        , m_jobs{jobs}
        , m_completionQueue{alloc}
        , m_cancelledIds{alloc}
    {
    }

    IOScheduler::~IOScheduler()
    {
        m_shutdown.store(true, std::memory_order_release);
        // Wait for in-flight IO jobs to complete before destroying members
        while (m_pendingCount.load(std::memory_order_acquire) > 0)
        {
            std::this_thread::yield();
        }
    }

    void IOScheduler::Shutdown()
    {
        m_shutdown.store(true, std::memory_order_release);
    }

    bool IOScheduler::IsShutdown() const noexcept
    {
        return m_shutdown.load(std::memory_order_acquire);
    }

    IORequestId IOScheduler::Submit(wax::StringView path, LoadPriority priority)
    {
        HIVE_PROFILE_SCOPE_N("IOScheduler::Submit");

        IORequestId id = m_nextId.fetch_add(1, std::memory_order_relaxed);
        m_pendingCount.fetch_add(1, std::memory_order_relaxed);

        // Allocate job data from the thread-safe allocator (survives across frames)
        void* mem = m_alloc->Allocate(sizeof(IOJobData), alignof(IOJobData));
        auto* jobData = new (mem) IOJobData{};
        jobData->m_self = this;
        jobData->m_id = id;
        jobData->m_path = wax::String{*m_alloc};
        jobData->m_path.Append(path.Data(), path.Size());

        (void)priority; // IO jobs are always LOW priority in the job system

        drone::JobDecl job;
        job.m_func = &IOScheduler::ExecuteIOJob;
        job.m_userData = jobData;
        job.m_priority = drone::Priority::LOW;

        m_jobs.SubmitDetached(job);
        return id;
    }

    void IOScheduler::Cancel(IORequestId id)
    {
        std::lock_guard<std::mutex> lock{m_cancelMutex};
        m_cancelledIds.Insert(id);
    }

    size_t IOScheduler::DrainCompletions(wax::Vector<IOCompletion>& out)
    {
        HIVE_PROFILE_SCOPE_N("IOScheduler::DrainCompletions");

        wax::Vector<IOCompletion> raw{*m_alloc};
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_completionMutex};
            raw = static_cast<wax::Vector<IOCompletion>&&>(m_completionQueue);
            m_completionQueue = wax::Vector<IOCompletion>{*m_alloc};
        }

        {
            std::lock_guard<std::mutex> lock{m_cancelMutex};
            for (size_t i = 0; i < raw.Size(); ++i)
            {
                if (m_cancelledIds.Contains(raw[i].m_requestId))
                {
                    m_cancelledIds.Remove(raw[i].m_requestId);
                }
                else
                {
                    out.PushBack(static_cast<IOCompletion&&>(raw[i]));
                }
            }
        }

        return out.Size();
    }

    drone::Task<wax::ByteBuffer> IOScheduler::ReadAsync(wax::StringView path)
    {
        co_await drone::ScheduleOn(m_jobs, drone::Priority::LOW);
        HIVE_PROFILE_SCOPE_N("IOScheduler::ReadAsync");
        co_return m_vfs->ReadSync(path);
    }

    size_t IOScheduler::PendingCount() const
    {
        return m_pendingCount.load(std::memory_order_relaxed);
    }

    void IOScheduler::ExecuteIOJob(void* data)
    {
        HIVE_PROFILE_SCOPE_N("IOScheduler::ReadFile");
        auto* jobData = static_cast<IOJobData*>(data);
        auto* self = jobData->m_self;

        auto buffer = self->m_vfs->ReadSync(jobData->m_path.View());

        IOCompletion completion;
        completion.m_requestId = jobData->m_id;
        completion.m_success = (buffer.Size() > 0);
        completion.m_data = static_cast<wax::ByteBuffer&&>(buffer);

        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{self->m_completionMutex};
            self->m_completionQueue.PushBack(static_cast<IOCompletion&&>(completion));
        }

        self->m_pendingCount.fetch_sub(1, std::memory_order_relaxed);

        // Free job data
        jobData->~IOJobData();
        self->m_alloc->Deallocate(jobData);
    }
} // namespace nectar
