#pragma once

#include <hive/hive_config.h>

#include <cstddef>
#include <cstdint>

namespace queen
{
    struct SchedulerTaskSlot
    {
        void* m_data[8];
    };

    static constexpr size_t kMaxSchedulerTasks = 256;

    HIVE_API SchedulerTaskSlot* GetSchedulerTaskBuffer() noexcept;
    HIVE_API size_t AllocateSchedulerTaskSlot() noexcept;
} // namespace queen
