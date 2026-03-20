#pragma once

#include <cstddef>
#include <cstdint>

namespace drone
{
    // kMainThread (SIZE_MAX) = main thread, 0..N-1 = worker index.
    // JobSystem sets the index on worker entry and clears it on exit.
    struct WorkerContext
    {
        static constexpr size_t kMainThread = SIZE_MAX;

        [[nodiscard]] static size_t CurrentWorkerIndex() noexcept
        {
            return g_tCurrentWorkerIndex;
        }

        [[nodiscard]] static bool IsInWorker() noexcept
        {
            return g_tCurrentWorkerIndex != kMainThread;
        }

        static void SetCurrentWorkerIndex(size_t index) noexcept
        {
            g_tCurrentWorkerIndex = index;
        }

        static void ClearCurrentWorkerIndex() noexcept
        {
            g_tCurrentWorkerIndex = kMainThread;
        }

    private:
        inline static thread_local size_t g_tCurrentWorkerIndex = kMainThread;
    };
} // namespace drone
