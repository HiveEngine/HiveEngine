#pragma once

#include <hive/hive_config.h>

#include <drone/counter.h>

#include <cstddef>

namespace drone
{
    struct ParallelChunk
    {
        void (*m_func)(size_t, void*);
        void* m_userData;
        size_t m_begin;
        size_t m_end;
        Counter* m_counter;
    };

    static constexpr size_t kMaxParallelChunks = 1024;

    HIVE_API ParallelChunk* GetParallelChunkBuffer() noexcept;
    HIVE_API size_t AllocateParallelChunkSlots(size_t count) noexcept;
} // namespace drone
