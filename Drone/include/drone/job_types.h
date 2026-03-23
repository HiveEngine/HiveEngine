#pragma once

#include <hive/hive_config.h>

#include <cstdint>

namespace drone
{
    class Counter;

    enum class Priority : uint8_t
    {
        HIGH = 0,   // ECS systems, input — latency-critical
        NORMAL = 1, // Asset cooking, general work
        LOW = 2,    // IO completions, GC — background
        COUNT = 3
    };

    struct JobDecl
    {
        using Func = void (*)(void* userData);

        Func m_func{nullptr};
        void* m_userData{nullptr};
        Priority m_priority{Priority::NORMAL};
        Counter* m_counter{nullptr}; // Auto-decremented after execution (set by Submit)

        HIVE_API void Execute() const;

        [[nodiscard]] bool IsValid() const noexcept
        {
            return m_func != nullptr;
        }
    };
} // namespace drone
