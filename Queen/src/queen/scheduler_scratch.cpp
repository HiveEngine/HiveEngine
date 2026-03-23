#include <queen/scheduler/scheduler_scratch.h>

namespace queen
{
    static thread_local SchedulerTaskSlot s_tasks[kMaxSchedulerTasks];
    static thread_local size_t s_taskIdx = 0;

    SchedulerTaskSlot* GetSchedulerTaskBuffer() noexcept
    {
        return s_tasks;
    }

    size_t AllocateSchedulerTaskSlot() noexcept
    {
        size_t idx = s_taskIdx % kMaxSchedulerTasks;
        ++s_taskIdx;
        return idx;
    }
} // namespace queen
