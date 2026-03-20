#pragma once

#include <drone/counter.h>
#include <drone/job_submitter.h>
#include <drone/job_types.h>

#include <coroutine>

namespace drone
{
    // co_await AwaitCounter(jobs, counter) suspends until counter reaches zero.
    // Uses a LOW watcher job that blocks via Counter::Wait then resumes the coroutine.
    class AwaitCounter
    {
    public:
        AwaitCounter(const JobSubmitter& submitter, Counter& counter)
            : m_submitter{&submitter}
            , m_counter{&counter}
        {
        }

        [[nodiscard]] bool await_ready() const noexcept
        {
            return m_counter->IsDone();
        }

        void await_suspend(std::coroutine_handle<> handle) noexcept
        {
            m_handle = handle;

            JobDecl job;
            job.m_func = [](void* data) {
                auto* self = static_cast<AwaitCounter*>(data);
                self->m_counter->Wait();
                self->m_handle.resume();
            };
            job.m_userData = this;
            job.m_priority = Priority::LOW;

            m_submitter->SubmitDetached(job);
        }

        void await_resume() noexcept
        {
        }

    private:
        const JobSubmitter* m_submitter;
        Counter* m_counter;
        std::coroutine_handle<> m_handle;
    };
} // namespace drone
