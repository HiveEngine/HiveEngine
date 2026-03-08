#pragma once

#include <comb/allocator_concepts.h>

#include <queen/core/tick.h>
#include <queen/scheduler/dependency_graph.h>
#include <queen/system/system_storage.h>

namespace queen
{
    class World;

    /**
     * Sequential scheduler for systems
     *
     * SequentialScheduler executes systems in topologically sorted order,
     * respecting data dependencies. This is the simplest scheduler that
     * runs systems one at a time on the main thread.
     *
     * Use cases:
     * - Single-threaded execution
     * - Debugging (deterministic order)
     * - Baseline for comparing parallel schedulers
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ graph_: DependencyGraph                                         │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Build: O(N^2) where N = number of systems
     * - Update: O(N) per frame (runs all systems sequentially)
     *
     * Limitations:
     * - No parallel execution
     * - Systems run in registration order when no conflicts exist
     */
    template <comb::Allocator Allocator> class Scheduler
    {
    public:
        explicit Scheduler(Allocator& allocator)
            : m_graph{allocator}
        {
        }

        Scheduler(const Scheduler&) = delete;
        Scheduler& operator=(const Scheduler&) = delete;
        Scheduler(Scheduler&&) noexcept = default;
        Scheduler& operator=(Scheduler&&) noexcept = default;

        /**
         * Build/rebuild the dependency graph from system storage
         *
         * Call this after registering new systems or when the graph is dirty.
         */
        void Build(const SystemStorage<Allocator>& storage)
        {
            m_graph.Build(storage);
        }

        /**
         * Mark the graph as needing rebuild
         *
         * Call this when systems are added, removed, or modified.
         */
        void Invalidate() noexcept
        {
            m_graph.MarkDirty();
        }

        /**
         * Check if the graph needs rebuild
         */
        [[nodiscard]] bool NeedsRebuild() const noexcept
        {
            return m_graph.IsDirty();
        }

        /**
         * Run all systems in dependency order
         *
         * After all systems have executed, flushes command buffers from
         * the World's Commands collection to apply deferred mutations.
         * Updates each system's last_run_tick for change detection.
         *
         * @param world The world to run systems on
         * @param storage System storage containing the systems
         */
        void RunAll(World& world, SystemStorage<Allocator>& storage); // Implementation in scheduler_impl.h

        /**
         * Get the dependency graph
         */
        [[nodiscard]] const DependencyGraph<Allocator>& Graph() const noexcept
        {
            return m_graph;
        }

        [[nodiscard]] DependencyGraph<Allocator>& Graph() noexcept
        {
            return m_graph;
        }

        /**
         * Get the execution order (for debugging/visualization)
         */
        [[nodiscard]] const wax::Vector<uint32_t>& ExecutionOrder() const noexcept
        {
            return m_graph.ExecutionOrder();
        }

        /**
         * Check if the dependency graph has cycles
         */
        [[nodiscard]] bool HasCycle() const noexcept
        {
            return m_graph.HasCycle();
        }

    private:
        DependencyGraph<Allocator> m_graph;
    };
} // namespace queen
