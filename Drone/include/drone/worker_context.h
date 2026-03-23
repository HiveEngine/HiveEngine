#pragma once

#include <hive/hive_config.h>

#include <cstddef>
#include <cstdint>

namespace drone
{
    struct HIVE_API WorkerContext
    {
        static constexpr size_t kMainThread = SIZE_MAX;

        [[nodiscard]] static size_t CurrentWorkerIndex() noexcept;
        [[nodiscard]] static bool IsInWorker() noexcept;
        static void SetCurrentWorkerIndex(size_t index) noexcept;
        static void ClearCurrentWorkerIndex() noexcept;
    };
} // namespace drone
