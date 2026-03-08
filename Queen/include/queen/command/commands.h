#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/command/command_buffer.h>

#include <thread>

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
        struct ThreadBuffer
        {
            std::thread::id m_threadId;
            CommandBuffer<Allocator> m_buffer;

            explicit ThreadBuffer(Allocator& alloc)
                : m_threadId{}
                , m_buffer{alloc} {}

            ThreadBuffer(std::thread::id id, Allocator& alloc)
                : m_threadId{id}
                , m_buffer{alloc} {}
        };

        explicit Commands(Allocator& allocator, size_t maxThreads = 16)
            : m_allocator{&allocator}
            , m_threadBuffers{allocator}
            , m_bufferCount{0} {
            m_threadBuffers.Reserve(maxThreads);
        }

        ~Commands() = default;

        Commands(const Commands&) = delete;
        Commands& operator=(const Commands&) = delete;
        Commands(Commands&&) = default;
        Commands& operator=(Commands&&) = default;

        /**
         * Get the command buffer for the current thread
         *
         * Creates a new buffer if this is the first access from this thread.
         *
         * @return Reference to thread-local CommandBuffer
         */
        [[nodiscard]] CommandBuffer<Allocator>& Get() {
            std::thread::id currentId = std::this_thread::get_id();

            for (size_t i = 0; i < m_bufferCount; ++i)
            {
                if (m_threadBuffers[i].m_threadId == currentId)
                {
                    return m_threadBuffers[i].m_buffer;
                }
            }

            return CreateBuffer(currentId);
        }

        /**
         * Get the command buffer for the current thread (const version)
         *
         * Asserts if no buffer exists for this thread.
         */
        [[nodiscard]] const CommandBuffer<Allocator>& Get() const {
            std::thread::id currentId = std::this_thread::get_id();

            for (size_t i = 0; i < m_bufferCount; ++i)
            {
                if (m_threadBuffers[i].m_threadId == currentId)
                {
                    return m_threadBuffers[i].m_buffer;
                }
            }

            hive::Assert(false, "No command buffer exists for this thread");
            // Unreachable, but needed for compilation
            return m_threadBuffers[0].m_buffer;
        }

        /**
         * Flush all thread-local command buffers to the World
         *
         * Must be called from a single thread (not during parallel execution).
         * Buffers are applied in deterministic order (by thread index).
         *
         * @param world The World to apply commands to
         */
        void FlushAll(World& world) {
            // Apply buffers in deterministic order (index order)
            for (size_t i = 0; i < m_bufferCount; ++i)
            {
                if (!m_threadBuffers[i].m_buffer.IsEmpty())
                {
                    m_threadBuffers[i].m_buffer.Flush(world);
                }
            }
        }

        /**
         * Clear all command buffers without applying them
         */
        void ClearAll() {
            for (size_t i = 0; i < m_bufferCount; ++i)
            {
                m_threadBuffers[i].m_buffer.Clear();
            }
        }

        /**
         * Get the number of active thread buffers
         */
        [[nodiscard]] size_t BufferCount() const noexcept { return m_bufferCount; }

        /**
         * Get total command count across all buffers
         */
        [[nodiscard]] size_t TotalCommandCount() const noexcept {
            size_t total = 0;
            for (size_t i = 0; i < m_bufferCount; ++i)
            {
                total += m_threadBuffers[i].m_buffer.CommandCount();
            }
            return total;
        }

        /**
         * Check if all buffers are empty
         */
        [[nodiscard]] bool IsEmpty() const noexcept {
            for (size_t i = 0; i < m_bufferCount; ++i)
            {
                if (!m_threadBuffers[i].m_buffer.IsEmpty())
                {
                    return false;
                }
            }
            return true;
        }

        /**
         * Iterate over all buffers (for advanced use)
         */
        template <typename Func> void ForEach(Func&& func) {
            for (size_t i = 0; i < m_bufferCount; ++i)
            {
                func(m_threadBuffers[i].m_buffer);
            }
        }

    private:
        CommandBuffer<Allocator>& CreateBuffer(std::thread::id id) {
            m_threadBuffers.EmplaceBack(id, *m_allocator);
            return m_threadBuffers[m_bufferCount++].m_buffer;
        }

        Allocator* m_allocator;
        wax::Vector<ThreadBuffer> m_threadBuffers;
        size_t m_bufferCount;
    };
} // namespace queen
