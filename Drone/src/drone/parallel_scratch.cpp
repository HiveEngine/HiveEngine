#include <drone/parallel_scratch.h>

namespace drone
{
    static thread_local ParallelChunk s_chunks[kMaxParallelChunks];
    static thread_local size_t s_chunkIdx = 0;

    ParallelChunk* GetParallelChunkBuffer() noexcept
    {
        return s_chunks;
    }

    size_t AllocateParallelChunkSlots(size_t count) noexcept
    {
        size_t base = s_chunkIdx % kMaxParallelChunks;
        s_chunkIdx += count;
        return base;
    }
} // namespace drone
