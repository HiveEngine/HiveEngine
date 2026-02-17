#pragma once

#include <queen/scheduler/system_node.h>
#include <queen/system/system_id.h>
#include <queen/system/access_descriptor.h>
#include <wax/containers/vector.h>
#include <comb/allocator_concepts.h>

namespace queen
{
    template<comb::Allocator Allocator>
    class SystemStorage;

    /**
     * Graph of system dependencies for scheduling
     *
     * DependencyGraph builds and maintains a directed acyclic graph (DAG)
     * of system dependencies. Dependencies are inferred automatically from
     * access patterns (AccessDescriptor) or can be specified explicitly.
     *
     * Use cases:
     * - Computing safe parallel execution order
     * - Detecting systems that can run concurrently
     * - Validating no cycles exist in dependencies
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ nodes_: Vector<SystemNode>                                      │
     * │ adjacency_: Vector<Vector<uint32_t>> (dependents per node)      │
     * │ roots_: Vector<uint32_t> (systems with no dependencies)         │
     * │ execution_order_: Vector<uint32_t> (topologically sorted)       │
     * │ dirty_: bool (needs rebuild)                                    │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Build: O(N^2) where N = number of systems
     * - Topological sort: O(N + E) where E = number of edges
     * - Reset: O(N)
     */
    template<comb::Allocator Allocator>
    class DependencyGraph
    {
    public:
        explicit DependencyGraph(Allocator& allocator)
            : nodes_{allocator}
            , adjacency_{allocator}
            , roots_{allocator}
            , execution_order_{allocator}
            , allocator_{allocator}
            , dirty_{true}
        {}

        DependencyGraph(const DependencyGraph&) = delete;
        DependencyGraph& operator=(const DependencyGraph&) = delete;
        DependencyGraph(DependencyGraph&&) noexcept = default;
        DependencyGraph& operator=(DependencyGraph&&) noexcept = default;

        /**
         * Build dependency graph from system storage
         *
         * Analyzes all registered systems' access patterns and builds
         * edges where systems conflict. Systems registered earlier take
         * precedence (run first) when conflicts occur.
         */
        void Build(const SystemStorage<Allocator>& storage)
        {
            Clear();

            const size_t system_count = storage.SystemCount();
            if (system_count == 0)
            {
                dirty_ = false;
                return;
            }

            // Create nodes for each system
            nodes_.Reserve(system_count);
            adjacency_.Reserve(system_count);

            for (size_t i = 0; i < system_count; ++i)
            {
                const auto* system = storage.GetSystemByIndex(i);
                if (system != nullptr)
                {
                    nodes_.PushBack(SystemNode{system->Id()});
                    adjacency_.PushBack(wax::Vector<uint32_t, Allocator>{allocator_});
                }
            }

            // Build edges based on access conflicts
            // System A -> System B means A must run before B
            for (size_t i = 0; i < system_count; ++i)
            {
                const auto* system_i = storage.GetSystemByIndex(i);
                if (system_i == nullptr) continue;

                uint16_t dep_count = 0;

                for (size_t j = 0; j < i; ++j)
                {
                    const auto* system_j = storage.GetSystemByIndex(j);
                    if (system_j == nullptr) continue;

                    // Check if systems conflict
                    if (system_i->Access().ConflictsWith(system_j->Access()))
                    {
                        // Earlier system (j) must run before later system (i)
                        // Add i as dependent of j
                        adjacency_[j].PushBack(static_cast<uint32_t>(i));
                        ++dep_count;
                    }
                }

                nodes_[i].SetDependencyCount(dep_count);
            }

            // Add explicit ordering edges (After/Before constraints)
            for (size_t i = 0; i < system_count; ++i)
            {
                const auto* system = storage.GetSystemByIndex(i);
                if (system == nullptr) continue;

                // After(id) means: this system must run after id
                for (uint8_t a = 0; a < system->AfterCount(); ++a)
                {
                    SystemId after_id = system->AfterDep(a);
                    uint32_t after_idx = after_id.Index();
                    if (after_idx < system_count)
                    {
                        adjacency_[after_idx].PushBack(static_cast<uint32_t>(i));
                        nodes_[i].IncrementDependencyCount();
                    }
                }

                // Before(id) means: this system must run before id
                for (uint8_t b = 0; b < system->BeforeCount(); ++b)
                {
                    SystemId before_id = system->BeforeDep(b);
                    uint32_t before_idx = before_id.Index();
                    if (before_idx < system_count)
                    {
                        adjacency_[i].PushBack(before_idx);
                        nodes_[before_idx].IncrementDependencyCount();
                    }
                }
            }

            // Find root systems (no dependencies)
            for (size_t i = 0; i < nodes_.Size(); ++i)
            {
                if (nodes_[i].DependencyCount() == 0)
                {
                    roots_.PushBack(static_cast<uint32_t>(i));
                }
            }

            // Compute topological order
            ComputeTopologicalOrder();

            dirty_ = false;
        }

        /**
         * Reset all nodes to pending state for a new frame
         */
        void Reset()
        {
            for (size_t i = 0; i < nodes_.Size(); ++i)
            {
                nodes_[i].Reset();
            }
        }

        /**
         * Mark graph as needing rebuild
         */
        void MarkDirty() noexcept { dirty_ = true; }

        /**
         * Check if graph needs rebuild
         */
        [[nodiscard]] bool IsDirty() const noexcept { return dirty_; }

        /**
         * Get the topologically sorted execution order
         */
        [[nodiscard]] const wax::Vector<uint32_t, Allocator>& ExecutionOrder() const noexcept
        {
            return execution_order_;
        }

        /**
         * Get root systems (no dependencies)
         */
        [[nodiscard]] const wax::Vector<uint32_t, Allocator>& Roots() const noexcept
        {
            return roots_;
        }

        /**
         * Get node by index
         */
        [[nodiscard]] SystemNode* GetNode(uint32_t index)
        {
            if (index < nodes_.Size())
            {
                return &nodes_[index];
            }
            return nullptr;
        }

        [[nodiscard]] const SystemNode* GetNode(uint32_t index) const
        {
            if (index < nodes_.Size())
            {
                return &nodes_[index];
            }
            return nullptr;
        }

        /**
         * Get dependents of a node (systems that depend on this one)
         */
        [[nodiscard]] const wax::Vector<uint32_t, Allocator>* GetDependents(uint32_t index) const
        {
            if (index < adjacency_.Size())
            {
                return &adjacency_[index];
            }
            return nullptr;
        }

        /**
         * Get number of nodes
         */
        [[nodiscard]] size_t NodeCount() const noexcept { return nodes_.Size(); }

        /**
         * Check if graph has cycles (should never happen with proper construction)
         */
        [[nodiscard]] bool HasCycle() const noexcept
        {
            // If topological sort succeeded, we have all nodes in order
            return execution_order_.Size() != nodes_.Size();
        }

    private:
        void Clear()
        {
            nodes_.Clear();
            adjacency_.Clear();
            roots_.Clear();
            execution_order_.Clear();
        }

        void ComputeTopologicalOrder()
        {
            execution_order_.Clear();
            execution_order_.Reserve(nodes_.Size());

            // Use Kahn's algorithm
            // Copy in-degree counts (we'll decrement them)
            wax::Vector<uint16_t, Allocator> in_degree{allocator_};
            in_degree.Reserve(nodes_.Size());
            for (size_t i = 0; i < nodes_.Size(); ++i)
            {
                in_degree.PushBack(nodes_[i].DependencyCount());
            }

            // Start with roots (in-degree 0)
            wax::Vector<uint32_t, Allocator> queue{allocator_};
            queue.Reserve(nodes_.Size());
            for (size_t i = 0; i < roots_.Size(); ++i)
            {
                queue.PushBack(roots_[i]);
            }

            size_t queue_front = 0;

            while (queue_front < queue.Size())
            {
                uint32_t current = queue[queue_front++];
                execution_order_.PushBack(current);

                // Reduce in-degree of dependents
                const auto& dependents = adjacency_[current];
                for (size_t i = 0; i < dependents.Size(); ++i)
                {
                    uint32_t dep = dependents[i];
                    if (in_degree[dep] > 0)
                    {
                        --in_degree[dep];
                        if (in_degree[dep] == 0)
                        {
                            queue.PushBack(dep);
                        }
                    }
                }
            }
        }

        wax::Vector<SystemNode, Allocator> nodes_;
        wax::Vector<wax::Vector<uint32_t, Allocator>, Allocator> adjacency_;
        wax::Vector<uint32_t, Allocator> roots_;
        wax::Vector<uint32_t, Allocator> execution_order_;
        Allocator& allocator_;
        bool dirty_;
    };
}
