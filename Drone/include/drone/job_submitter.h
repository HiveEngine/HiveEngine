#pragma once

#include <drone/counter.h>
#include <drone/job_types.h>

#include <cstddef>

namespace drone
{

    // Type-erased job submission. Lets non-templated code (Nectar, etc.)
    // submit without knowing the JobSystem's allocator type.
    class JobSubmitter
    {
    public:
        using SubmitFn = void (*)(void* ctx, const JobDecl* jobs, size_t count, Counter* counter);
        using ParForFn = void (*)(void* ctx, size_t begin, size_t end, void (*func)(size_t, void*), void* data,
                                  size_t chunkSize);
        using WorkerCountFn = size_t (*)(void* ctx);

        JobSubmitter() = default;

        JobSubmitter(void* ctx, SubmitFn submit, ParForFn parFor, WorkerCountFn workerCount)
            : m_ctx{ctx}
            , m_submit{submit}
            , m_parFor{parFor}
            , m_workerCount{workerCount}
        {
        }

        void Submit(JobDecl job, Counter& counter) const
        {
            m_submit(m_ctx, &job, 1, &counter);
        }

        void Submit(const JobDecl* jobs, size_t count, Counter& counter) const
        {
            m_submit(m_ctx, jobs, count, &counter);
        }

        void SubmitDetached(JobDecl job) const
        {
            m_submit(m_ctx, &job, 1, nullptr);
        }

        void ParallelFor(size_t begin, size_t end, void (*func)(size_t, void*), void* data, size_t chunkSize = 0) const
        {
            m_parFor(m_ctx, begin, end, func, data, chunkSize);
        }

        [[nodiscard]] size_t WorkerCount() const
        {
            return m_workerCount(m_ctx);
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return m_ctx != nullptr;
        }

    private:
        void* m_ctx{nullptr};
        SubmitFn m_submit{nullptr};
        ParForFn m_parFor{nullptr};
        WorkerCountFn m_workerCount{nullptr};
    };

} // namespace drone
