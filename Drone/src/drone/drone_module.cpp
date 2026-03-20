#include <drone/counter.h>
#include <drone/job_submitter.h>
#include <drone/job_system.h>
#include <drone/job_types.h>
#include <drone/mpmc_queue.h>
#include <drone/schedule_on.h>
#include <drone/task.h>
#include <drone/work_stealing_deque.h>
#include <drone/worker_context.h>

namespace drone
{
    void JobDecl::Execute() const
    {
        if (m_func != nullptr)
        {
            m_func(m_userData);
        }
        if (m_counter != nullptr)
        {
            m_counter->Decrement();
        }
    }
} // namespace drone
