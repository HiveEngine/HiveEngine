#include <drone/worker_context.h>

namespace drone
{
    static thread_local size_t g_tCurrentWorkerIndex = WorkerContext::kMainThread;

    size_t WorkerContext::CurrentWorkerIndex() noexcept
    {
        return g_tCurrentWorkerIndex;
    }

    bool WorkerContext::IsInWorker() noexcept
    {
        return g_tCurrentWorkerIndex != kMainThread;
    }

    void WorkerContext::SetCurrentWorkerIndex(size_t index) noexcept
    {
        g_tCurrentWorkerIndex = index;
    }

    void WorkerContext::ClearCurrentWorkerIndex() noexcept
    {
        g_tCurrentWorkerIndex = kMainThread;
    }
} // namespace drone
