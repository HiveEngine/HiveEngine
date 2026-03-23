#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/command/command_buffer.h>

#include <drone/worker_context.h>

namespace queen
{
    /**
     * Thread-local command buffer collection for deferred mutations
     *
     * Commands provides per-thread CommandBuffers for safe structural mutations
     * during parallel system execution. Each thread gets its own buffer to avoid
     * contention. All buffers are flushed atomically at sync points.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────────┐
     * │ allocator_: Allocator& (shared allocator for all buffers)        │
     * │ thread_buffers_: Vector<ThreadBuffer> (one per active thread)    │
     * │ buffer_count_: size_t (number of buffers in use)                 │
     * └──────────────────────────────────────────────────────────────────┘
     *
     * ThreadBuffer structure:
     * ┌──────────────────────────────────────────────────────────────────┐
     * │ thread_id: std::thread::id                                       │
     * │ buffer: CommandBuffer<Allocator>                                 │
     * └──────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Get(): O(n) where n = active threads (linear search, cached)
     * - FlushAll(): O(c) where c = total commands across all buffers
     * - Thread-safe: Yes (each thread has its own buffer)
     *
     * Use cases:
     * - Deferred spawn/despawn during query iteration
     * - Parallel system structural mutations
     * - Avoiding iterator invalidation during Each()
     *
     * Limitations:
     * - FlushAll must be called from single thread (not parallel)
     * - Thread registration is not thread-safe (call Get from same threads consistently)
     * - Maximum number of threads is limited by initial capacity
     *
     * Example:
     * @code
     *   // In a system
     *   world.System<Read<Health>>("DeathCheck")
     *       .WithCommands()
     *       .EachWithCommands([](Entity e, const Health& hp, Commands<Allocator>& cmd) {
     *           if (hp.value <= 0) {
     *               cmd.Get().Despawn(e);  // Deferred
     *           }
     *       });
     *
     *   // Scheduler automatically flushes at sync point
     *   world.Update();  // Commands applied after all systems run
     * @endcode
     */
    template <comb::Allocator Allocator> class Commands
    {
    public:
        explicit Commands(Allocator& allocator, size_t workerCount = 0)
            : m_allocator{&allocator}
            , m_buffers{allocator}
        {
            // Slot 0 = main thread, slots 1..N = workers
            size_t slotCount = 1 + workerCount;
            m_buffers.Reserve(slotCount);
            for (size_t i = 0; i < slotCount; ++i)
            {
                m_buffers.EmplaceBack(allocator);
            }
        }

        ~Commands() = default;

        Commands(const Commands&) = delete;
        Commands& operator=(const Commands&) = delete;
        Commands(Commands&&) = default;
        Commands& operator=(Commands&&) = default;

        /**
         * Get the command buffer for the current thread
         *
         * Uses WorkerContext index for O(1) lookup. Slot 0 is reserved for
         * the main thread, slots 1..N for worker threads.
         *
         * @return Reference to per-thread CommandBuffer
         */
        [[nodiscard]] CommandBuffer<Allocator>& Get()
        {
            return m_buffers[SlotIndex()];
        }

        [[nodiscard]] const CommandBuffer<Allocator>& Get() const
        {
            return m_buffers[SlotIndex()];
        }

        /**
         * Flush all thread-local command buffers to the World
         *
         * Must be called from a single thread (not during parallel execution).
         * Buffers are applied in deterministic order (by thread index).
         *
         * @param world The World to apply commands to
         */
        void FlushAll(World& world)
        {
            for (size_t i = 0; i < m_buffers.Size(); ++i)
            {
                if (!m_buffers[i].IsEmpty())
                {
                    m_buffers[i].Flush(world);
                }
            }
        }

        void ClearAll()
        {
            for (size_t i = 0; i < m_buffers.Size(); ++i)
            {
                m_buffers[i].Clear();
            }
        }

        [[nodiscard]] size_t BufferCount() const noexcept
        {
            return m_buffers.Size();
        }

        [[nodiscard]] size_t TotalCommandCount() const noexcept
        {
            size_t total = 0;
            for (size_t i = 0; i < m_buffers.Size(); ++i)
            {
                total += m_buffers[i].CommandCount();
            }
            return total;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            for (size_t i = 0; i < m_buffers.Size(); ++i)
            {
                if (!m_buffers[i].IsEmpty())
                {
                    return false;
                }
            }
            return true;
        }

        template <typename Func> void ForEach(Func&& func)
        {
            for (size_t i = 0; i < m_buffers.Size(); ++i)
            {
                func(m_buffers[i]);
            }
        }

    private:
        [[nodiscard]] static size_t SlotIndex()
        {
            size_t idx = drone::WorkerContext::CurrentWorkerIndex();
            return (idx == drone::WorkerContext::kMainThread) ? 0 : idx + 1;
        }

        Allocator* m_allocator;
        wax::Vector<CommandBuffer<Allocator>> m_buffers;
    };
} // namespace queen
