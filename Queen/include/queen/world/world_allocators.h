#pragma once

#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>
#include <comb/buddy_allocator.h>
#include <comb/linear_allocator.h>
#include <comb/thread_safe_allocator.h>

#include <wax/containers/vector.h>

#include <mutex>

namespace queen
{
    /**
     * Thread-safe allocator wrapper that satisfies the comb::Allocator concept
     *
     * This wrapper holds a reference to a BuddyAllocator and a mutex,
     * providing thread-safe allocation for use during parallel execution.
     */
    class ThreadSafeBuddyAllocator
    {
    public:
        using MutexType = HIVE_PROFILE_LOCKABLE_BASE(std::mutex);

        ThreadSafeBuddyAllocator(comb::BuddyAllocator& allocator, MutexType& mutex)
            : m_allocator{&allocator}
            , m_mutex{&mutex}
        {
        }

        [[nodiscard]] void* Allocate(size_t size, size_t alignment, const char* tag = nullptr)
        {
            std::lock_guard<MutexType> lock{*m_mutex};
            return m_allocator->Allocate(size, alignment, tag);
        }

        void Deallocate(void* ptr)
        {
            std::lock_guard<MutexType> lock{*m_mutex};
            m_allocator->Deallocate(ptr);
        }

        [[nodiscard]] size_t GetUsedMemory() const
        {
            std::lock_guard<MutexType> lock{*m_mutex};
            return m_allocator->GetUsedMemory();
        }

        [[nodiscard]] size_t GetTotalMemory() const
        {
            std::lock_guard<MutexType> lock{*m_mutex};
            return m_allocator->GetTotalMemory();
        }

        [[nodiscard]] const char* GetName() const
        {
            return "ThreadSafeBuddyAllocator";
        }

    private:
        comb::BuddyAllocator* m_allocator;
        MutexType* m_mutex;
    };

    /**
     * Memory allocation strategy for ECS World
     *
     * Separates allocations by lifetime to optimize memory usage:
     * - Persistent: Long-lived data (archetypes, systems, graphs)
     * - Components: Entity component data (tables, columns)
     * - Frame: Per-frame temporary data (commands, query cache)
     * - Thread: Per-thread temporary data (parallel execution)
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────────┐
     * │ Backing Memory (provided by user)                                  │
     * │ ┌──────────────────────────────────────────────────────────────┐   │
     * │ │ Persistent (BuddyAllocator)                                  │   │
     * │ │ - Archetypes, ArchetypeGraph, ComponentIndex                 │   │
     * │ │ - Systems, Scheduler, DependencyGraph                        │   │
     * │ │ - Resources                                                  │   │
     * │ └──────────────────────────────────────────────────────────────┘   │
     * │ ┌──────────────────────────────────────────────────────────────┐   │
     * │ │ Components (BuddyAllocator)                                  │   │
     * │ │ - Table column data                                          │   │
     * │ │ - Entity location map                                        │   │
     * │ │ - EntityAllocator data                                       │   │
     * │ └──────────────────────────────────────────────────────────────┘   │
     * │ ┌──────────────────────────────────────────────────────────────┐   │
     * │ │ Frame (LinearAllocator) - Reset each Update()                │   │
     * │ │ - Command buffers                                            │   │
     * │ │ - Temporary query results                                    │   │
     * │ └──────────────────────────────────────────────────────────────┘   │
     * │ ┌──────────────────────────────────────────────────────────────┐   │
     * │ │ Thread[0..N] (LinearAllocator per thread)                    │   │
     * │ │ - Per-system temporary allocations                           │   │
     * │ │ - Parallel task data                                         │   │
     * │ └──────────────────────────────────────────────────────────────┘   │
     * └────────────────────────────────────────────────────────────────────┘
     */

    struct WorldAllocatorConfig
    {
        size_t m_persistentSize = 32 * 1024 * 1024;  // 32 MB — archetypes, systems, resources
        size_t m_componentSize = 128 * 1024 * 1024;  // 128 MB — entity component data
        size_t m_frameSize = 8 * 1024 * 1024;        // 8 MB — per-frame temporaries
        size_t m_threadFrameSize = 512 * 1024;       // 512 KB per worker thread
        size_t m_threadCount = 0;                    // 0 = auto-detect
    };

    /**
     * Collection of specialized allocators for World
     *
     * This class owns and manages all allocators used by the ECS World.
     * It provides proper lifetime management and convenient accessors.
     */
    class WorldAllocators
    {
    public:
        WorldAllocators(size_t persistentSize, size_t componentSize, size_t frameSize, size_t threadFrameSize,
                        size_t threadCount)
            : m_persistent{persistentSize}
            , m_components{componentSize}
            , m_frame{frameSize}
            , m_threadFrames{m_persistent} // Use persistent for the vector itself
        {
            if (threadCount == 0)
            {
                threadCount = std::thread::hardware_concurrency();
                if (threadCount == 0)
                    threadCount = 4; // Fallback
            }

            m_threadFrames.Reserve(threadCount);
            for (size_t i = 0; i < threadCount; ++i)
            {
                m_threadFrames.EmplaceBack(threadFrameSize);
            }
        }

        ~WorldAllocators() = default;

        // Not copyable or movable (contains mutex)
        WorldAllocators(const WorldAllocators&) = delete;
        WorldAllocators& operator=(const WorldAllocators&) = delete;
        WorldAllocators(WorldAllocators&&) = delete;
        WorldAllocators& operator=(WorldAllocators&&) = delete;

        /**
         * Create WorldAllocators with default configuration
         */
        static WorldAllocators Create()
        {
            return Create(WorldAllocatorConfig{});
        }

        /**
         * Create WorldAllocators with custom configuration
         */
        static WorldAllocators Create(const WorldAllocatorConfig& config)
        {
            return WorldAllocators{config.m_persistentSize, config.m_componentSize, config.m_frameSize,
                                   config.m_threadFrameSize, config.m_threadCount};
        }

        // Allocator Accessors

        /**
         * Persistent allocator for long-lived data
         *
         * WARNING: Not thread-safe! Use PersistentThreadSafe() for parallel execution.
         *
         * Use for: Archetypes, Systems, ArchetypeGraph, ComponentIndex,
         *          Resources, DependencyGraph
         */
        [[nodiscard]] comb::BuddyAllocator& Persistent() noexcept
        {
            return m_persistent;
        }

        [[nodiscard]] const comb::BuddyAllocator& Persistent() const noexcept
        {
            return m_persistent;
        }

        /**
         * Thread-safe wrapper around the persistent allocator
         *
         * Use this for allocations during parallel system execution.
         * Returns a lightweight wrapper that locks the internal mutex.
         */
        [[nodiscard]] ThreadSafeBuddyAllocator PersistentThreadSafe() noexcept
        {
            return ThreadSafeBuddyAllocator{m_persistent, m_persistentMutex};
        }

        /**
         * Access the persistent allocator's mutex for external locking
         *
         * Use this when you need to protect a sequence of operations
         * on the persistent allocator (e.g., Query construction).
         */
        [[nodiscard]] ThreadSafeBuddyAllocator::MutexType& PersistentMutex() noexcept
        {
            return m_persistentMutex;
        }

        /**
         * Component allocator for entity data
         *
         * Use for: Table columns, EntityAllocator, EntityLocationMap
         */
        [[nodiscard]] comb::BuddyAllocator& Components() noexcept
        {
            return m_components;
        }

        [[nodiscard]] const comb::BuddyAllocator& Components() const noexcept
        {
            return m_components;
        }

        /**
         * Frame allocator for per-frame temporary data
         *
         * Use for: CommandBuffers, temporary query results
         * Reset at end of each Update()
         */
        [[nodiscard]] comb::LinearAllocator& Frame() noexcept
        {
            return m_frame;
        }

        [[nodiscard]] const comb::LinearAllocator& Frame() const noexcept
        {
            return m_frame;
        }

        /**
         * Get per-thread frame allocator
         *
         * Use for: Per-system temporary allocations during parallel execution
         * Reset at end of each system execution
         *
         * @param thread_index Index of the worker thread (0 to ThreadCount()-1)
         */
        [[nodiscard]] comb::LinearAllocator& ThreadFrame(size_t threadIndex) noexcept
        {
            hive::Assert(threadIndex < m_threadFrames.Size(), "Thread index out of bounds");
            return m_threadFrames[threadIndex];
        }

        [[nodiscard]] const comb::LinearAllocator& ThreadFrame(size_t threadIndex) const noexcept
        {
            hive::Assert(threadIndex < m_threadFrames.Size(), "Thread index out of bounds");
            return m_threadFrames[threadIndex];
        }

        /**
         * Number of thread-local allocators available
         */
        [[nodiscard]] size_t ThreadCount() const noexcept
        {
            return m_threadFrames.Size();
        }

        // Reset Operations

        /**
         * Reset frame allocator
         *
         * Called automatically at the end of World::Update()
         */
        void ResetFrame() noexcept
        {
            m_frame.Reset();
        }

        /**
         * Reset all thread-local frame allocators
         *
         * Called automatically after parallel system execution
         */
        void ResetThreadFrames() noexcept
        {
            for (size_t i = 0; i < m_threadFrames.Size(); ++i)
            {
                m_threadFrames[i].Reset();
            }
        }

        /**
         * Reset a specific thread-local allocator
         */
        void ResetThreadFrame(size_t threadIndex) noexcept
        {
            hive::Assert(threadIndex < m_threadFrames.Size(), "Thread index out of bounds");
            m_threadFrames[threadIndex].Reset();
        }

        // Statistics

        /**
         * Get total memory used by persistent allocator
         */
        [[nodiscard]] size_t PersistentUsed() const noexcept
        {
            return m_persistent.GetUsedMemory();
        }

        /**
         * Get total memory used by component allocator
         */
        [[nodiscard]] size_t ComponentsUsed() const noexcept
        {
            return m_components.GetUsedMemory();
        }

        /**
         * Get current frame allocator usage
         */
        [[nodiscard]] size_t FrameUsed() const noexcept
        {
            return m_frame.GetUsedMemory();
        }

        /**
         * Get total capacity across all allocators
         */
        [[nodiscard]] size_t TotalCapacity() const noexcept
        {
            size_t total = m_persistent.GetTotalMemory() + m_components.GetTotalMemory() + m_frame.GetTotalMemory();

            for (size_t i = 0; i < m_threadFrames.Size(); ++i)
            {
                total += m_threadFrames[i].GetTotalMemory();
            }

            return total;
        }

        // Thread-Safe Allocation (for parallel system execution)

        /**
         * Thread-safe allocation from the persistent allocator
         *
         * Use this when allocating from worker threads during parallel execution.
         */
        [[nodiscard]] void* PersistentAllocateThreadSafe(size_t size, size_t alignment, const char* tag = nullptr)
        {
            std::lock_guard<ThreadSafeBuddyAllocator::MutexType> lock{m_persistentMutex};
            return m_persistent.Allocate(size, alignment, tag);
        }

        /**
         * Thread-safe deallocation from the persistent allocator
         */
        void PersistentDeallocateThreadSafe(void* ptr)
        {
            std::lock_guard<ThreadSafeBuddyAllocator::MutexType> lock{m_persistentMutex};
            m_persistent.Deallocate(ptr);
        }

    private:
        comb::BuddyAllocator m_persistent;
        comb::BuddyAllocator m_components;
        comb::LinearAllocator m_frame;
        wax::Vector<comb::LinearAllocator> m_threadFrames;
        mutable HIVE_PROFILE_LOCKABLE_N(std::mutex, m_persistentMutex,
                                        "PersistentAllocatorMutex"); // Protects persistent_ during parallel execution
    };
} // namespace queen
