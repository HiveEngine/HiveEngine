#pragma once

#include <comb/allocator_concepts.h>
#include <comb/linear_allocator.h>
#include <comb/buddy_allocator.h>
#include <comb/thread_safe_allocator.h>
#include <wax/containers/vector.h>
#include <hive/core/assert.h>
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
        ThreadSafeBuddyAllocator(comb::BuddyAllocator& allocator, std::mutex& mutex)
            : allocator_{&allocator}
            , mutex_{&mutex}
        {}

        [[nodiscard]] void* Allocate(size_t size, size_t alignment, const char* tag = nullptr)
        {
            std::lock_guard<std::mutex> lock{*mutex_};
            return allocator_->Allocate(size, alignment, tag);
        }

        void Deallocate(void* ptr)
        {
            std::lock_guard<std::mutex> lock{*mutex_};
            allocator_->Deallocate(ptr);
        }

        [[nodiscard]] size_t GetUsedMemory() const
        {
            std::lock_guard<std::mutex> lock{*mutex_};
            return allocator_->GetUsedMemory();
        }

        [[nodiscard]] size_t GetTotalMemory() const
        {
            std::lock_guard<std::mutex> lock{*mutex_};
            return allocator_->GetTotalMemory();
        }

        [[nodiscard]] const char* GetName() const
        {
            return "ThreadSafeBuddyAllocator";
        }

    private:
        comb::BuddyAllocator* allocator_;
        std::mutex* mutex_;
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
     *
     * Performance characteristics:
     * - Persistent/Components: O(log N) alloc/dealloc (buddy)
     * - Frame/Thread: O(1) alloc (linear bump), O(1) reset
     *
     * Usage:
     * @code
     *   // Option 1: Default sizes
     *   WorldAllocators allocs = WorldAllocators::Create(backing_alloc);
     *
     *   // Option 2: Custom sizes
     *   WorldAllocatorConfig config;
     *   config.persistent_size = 16_MB;
     *   config.component_size = 128_MB;
     *   config.frame_size = 2_MB;
     *   config.thread_frame_size = 512_KB;
     *   config.thread_count = 4;
     *   WorldAllocators allocs = WorldAllocators::Create(backing_alloc, config);
     *
     *   // In game loop
     *   world.Update();
     *   allocs.ResetFrame();  // Called automatically by World::Update()
     * @endcode
     */

    struct WorldAllocatorConfig
    {
        // Note: BuddyAllocator MaxLevels=20 supports up to 32 MB per allocator
        size_t persistent_size = 8 * 1024 * 1024;      // 8 MB default
        size_t component_size = 32 * 1024 * 1024;      // 32 MB default (max for BuddyAllocator)
        size_t frame_size = 1 * 1024 * 1024;           // 1 MB default
        size_t thread_frame_size = 256 * 1024;         // 256 KB per thread
        size_t thread_count = 0;                        // 0 = auto-detect
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
        WorldAllocators(
            size_t persistent_size,
            size_t component_size,
            size_t frame_size,
            size_t thread_frame_size,
            size_t thread_count)
            : persistent_{persistent_size}
            , components_{component_size}
            , frame_{frame_size}
            , thread_frames_{persistent_}  // Use persistent for the vector itself
        {
            // Auto-detect thread count if 0
            if (thread_count == 0)
            {
                thread_count = std::thread::hardware_concurrency();
                if (thread_count == 0) thread_count = 4;  // Fallback
            }

            // Create per-thread frame allocators
            thread_frames_.Reserve(thread_count);
            for (size_t i = 0; i < thread_count; ++i)
            {
                thread_frames_.EmplaceBack(thread_frame_size);
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
            return WorldAllocators{
                config.persistent_size,
                config.component_size,
                config.frame_size,
                config.thread_frame_size,
                config.thread_count
            };
        }

        // ─────────────────────────────────────────────────────────────
        // Allocator Accessors
        // ─────────────────────────────────────────────────────────────

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
            return persistent_;
        }

        [[nodiscard]] const comb::BuddyAllocator& Persistent() const noexcept
        {
            return persistent_;
        }

        /**
         * Thread-safe wrapper around the persistent allocator
         *
         * Use this for allocations during parallel system execution.
         * Returns a lightweight wrapper that locks the internal mutex.
         */
        [[nodiscard]] ThreadSafeBuddyAllocator PersistentThreadSafe() noexcept
        {
            return ThreadSafeBuddyAllocator{persistent_, persistent_mutex_};
        }

        /**
         * Access the persistent allocator's mutex for external locking
         *
         * Use this when you need to protect a sequence of operations
         * on the persistent allocator (e.g., Query construction).
         */
        [[nodiscard]] std::mutex& PersistentMutex() noexcept
        {
            return persistent_mutex_;
        }

        /**
         * Component allocator for entity data
         *
         * Use for: Table columns, EntityAllocator, EntityLocationMap
         */
        [[nodiscard]] comb::BuddyAllocator& Components() noexcept
        {
            return components_;
        }

        [[nodiscard]] const comb::BuddyAllocator& Components() const noexcept
        {
            return components_;
        }

        /**
         * Frame allocator for per-frame temporary data
         *
         * Use for: CommandBuffers, temporary query results
         * Reset at end of each Update()
         */
        [[nodiscard]] comb::LinearAllocator& Frame() noexcept
        {
            return frame_;
        }

        [[nodiscard]] const comb::LinearAllocator& Frame() const noexcept
        {
            return frame_;
        }

        /**
         * Get per-thread frame allocator
         *
         * Use for: Per-system temporary allocations during parallel execution
         * Reset at end of each system execution
         *
         * @param thread_index Index of the worker thread (0 to ThreadCount()-1)
         */
        [[nodiscard]] comb::LinearAllocator& ThreadFrame(size_t thread_index) noexcept
        {
            hive::Assert(thread_index < thread_frames_.Size(), "Thread index out of bounds");
            return thread_frames_[thread_index];
        }

        [[nodiscard]] const comb::LinearAllocator& ThreadFrame(size_t thread_index) const noexcept
        {
            hive::Assert(thread_index < thread_frames_.Size(), "Thread index out of bounds");
            return thread_frames_[thread_index];
        }

        /**
         * Number of thread-local allocators available
         */
        [[nodiscard]] size_t ThreadCount() const noexcept
        {
            return thread_frames_.Size();
        }

        // ─────────────────────────────────────────────────────────────
        // Reset Operations
        // ─────────────────────────────────────────────────────────────

        /**
         * Reset frame allocator
         *
         * Called automatically at the end of World::Update()
         */
        void ResetFrame() noexcept
        {
            frame_.Reset();
        }

        /**
         * Reset all thread-local frame allocators
         *
         * Called automatically after parallel system execution
         */
        void ResetThreadFrames() noexcept
        {
            for (size_t i = 0; i < thread_frames_.Size(); ++i)
            {
                thread_frames_[i].Reset();
            }
        }

        /**
         * Reset a specific thread-local allocator
         */
        void ResetThreadFrame(size_t thread_index) noexcept
        {
            hive::Assert(thread_index < thread_frames_.Size(), "Thread index out of bounds");
            thread_frames_[thread_index].Reset();
        }

        // ─────────────────────────────────────────────────────────────
        // Statistics
        // ─────────────────────────────────────────────────────────────

        /**
         * Get total memory used by persistent allocator
         */
        [[nodiscard]] size_t PersistentUsed() const noexcept
        {
            return persistent_.GetUsedMemory();
        }

        /**
         * Get total memory used by component allocator
         */
        [[nodiscard]] size_t ComponentsUsed() const noexcept
        {
            return components_.GetUsedMemory();
        }

        /**
         * Get current frame allocator usage
         */
        [[nodiscard]] size_t FrameUsed() const noexcept
        {
            return frame_.GetUsedMemory();
        }

        /**
         * Get total capacity across all allocators
         */
        [[nodiscard]] size_t TotalCapacity() const noexcept
        {
            size_t total = persistent_.GetTotalMemory()
                         + components_.GetTotalMemory()
                         + frame_.GetTotalMemory();

            for (size_t i = 0; i < thread_frames_.Size(); ++i)
            {
                total += thread_frames_[i].GetTotalMemory();
            }

            return total;
        }

        // ─────────────────────────────────────────────────────────────
        // Thread-Safe Allocation (for parallel system execution)
        // ─────────────────────────────────────────────────────────────

        /**
         * Thread-safe allocation from the persistent allocator
         *
         * Use this when allocating from worker threads during parallel execution.
         */
        [[nodiscard]] void* PersistentAllocateThreadSafe(size_t size, size_t alignment, const char* tag = nullptr)
        {
            std::lock_guard<std::mutex> lock{persistent_mutex_};
            return persistent_.Allocate(size, alignment, tag);
        }

        /**
         * Thread-safe deallocation from the persistent allocator
         */
        void PersistentDeallocateThreadSafe(void* ptr)
        {
            std::lock_guard<std::mutex> lock{persistent_mutex_};
            persistent_.Deallocate(ptr);
        }

    private:
        comb::BuddyAllocator persistent_;
        comb::BuddyAllocator components_;
        comb::LinearAllocator frame_;
        wax::Vector<comb::LinearAllocator, comb::BuddyAllocator> thread_frames_;
        mutable std::mutex persistent_mutex_;  // Protects persistent_ during parallel execution
    };
}
