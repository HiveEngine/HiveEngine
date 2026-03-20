#pragma once

#include <drone/job_submitter.h>
#include <drone/job_types.h>

#include <coroutine>

namespace drone
{
    // co_await ScheduleOn(jobs) suspends and resumes on a Drone worker.
    class ScheduleOn
    {
    public:
        explicit ScheduleOn(const JobSubmitter& submitter, Priority priority = Priority::NORMAL)
            : m_submitter{&submitter}
            , m_priority{priority}
        {
        }

        bool await_ready() noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) noexcept
        {
            JobDecl job;
            job.m_func = [](void* data) {
                auto h = std::coroutine_handle<>::from_address(data);
                h.resume();
            };
            job.m_userData = handle.address();
            job.m_priority = m_priority;

            m_submitter->SubmitDetached(job);
        }

        void await_resume() noexcept
        {
        }

    private:
        const JobSubmitter* m_submitter;
        Priority m_priority;
    };
} // namespace drone
