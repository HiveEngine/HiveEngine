#pragma once

#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/scheduler/system_node.h>
#include <queen/system/access_descriptor.h>
#include <queen/system/system_id.h>

namespace queen
{
    template <comb::Allocator Allocator> class SystemStorage;

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
    template <comb::Allocator Allocator> class DependencyGraph
    {
    public:
        explicit DependencyGraph(Allocator& allocator)
            : m_nodes{allocator}
            , m_adjacency{allocator}
            , m_roots{allocator}
            , m_executionOrder{allocator}
            , m_allocator{allocator}
            , m_dirty{true}
        {
        }

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
            HIVE_PROFILE_SCOPE_N("DependencyGraph::Build");
            Clear();

            const size_t systemCount = storage.SystemCount();
            if (systemCount == 0)
            {
                m_dirty = false;
                return;
            }

            m_nodes.Reserve(systemCount);
            m_adjacency.Reserve(systemCount);

            for (size_t i = 0; i < systemCount; ++i)
            {
                const auto* system = storage.GetSystemByIndex(i);
                if (system != nullptr)
                {
                    m_nodes.PushBack(SystemNode{system->Id()});
                    m_adjacency.PushBack(wax::Vector<uint32_t>{m_allocator});
                }
            }

            // System A -> System B means A must run before B
            for (size_t i = 0; i < systemCount; ++i)
            {
                const auto* systemI = storage.GetSystemByIndex(i);
                if (systemI == nullptr)
                    continue;

                uint16_t depCount = 0;

                for (size_t j = 0; j < i; ++j)
                {
                    const auto* systemJ = storage.GetSystemByIndex(j);
                    if (systemJ == nullptr)
                        continue;

                    // Check if systems conflict
                    if (systemI->Access().ConflictsWith(systemJ->Access()))
                    {
                        // Earlier system (j) must run before later system (i)
                        // Add i as dependent of j
                        m_adjacency[j].PushBack(static_cast<uint32_t>(i));
                        ++depCount;
                    }
                }

                m_nodes[i].SetDependencyCount(depCount);
            }

            for (size_t i = 0; i < systemCount; ++i)
            {
                const auto* system = storage.GetSystemByIndex(i);
                if (system == nullptr)
                    continue;

                // After(id) means: this system must run after id
                for (uint8_t a = 0; a < system->AfterCount(); ++a)
                {
                    SystemId afterId = system->AfterDep(a);
                    uint32_t afterIdx = afterId.Index();
                    if (afterIdx < systemCount && !HasEdge(afterIdx, static_cast<uint32_t>(i)))
                    {
                        m_adjacency[afterIdx].PushBack(static_cast<uint32_t>(i));
                        m_nodes[i].IncrementDependencyCount();
                    }
                }

                // Before(id) means: this system must run before id
                for (uint8_t b = 0; b < system->BeforeCount(); ++b)
                {
                    SystemId beforeId = system->BeforeDep(b);
                    uint32_t beforeIdx = beforeId.Index();
                    if (beforeIdx < systemCount && !HasEdge(static_cast<uint32_t>(i), beforeIdx))
                    {
                        m_adjacency[i].PushBack(beforeIdx);
                        m_nodes[beforeIdx].IncrementDependencyCount();
                    }
                }
            }

            for (size_t i = 0; i < m_nodes.Size(); ++i)
            {
                if (m_nodes[i].DependencyCount() == 0)
                {
                    m_roots.PushBack(static_cast<uint32_t>(i));
                }
            }

            ComputeTopologicalOrder();

            m_dirty = false;
        }

        /**
         * Reset all nodes to pending state for a new frame
         */
        void Reset()
        {
            for (size_t i = 0; i < m_nodes.Size(); ++i)
            {
                m_nodes[i].Reset();
            }
        }

        /**
         * Mark graph as needing rebuild
         */
        void MarkDirty() noexcept
        {
            m_dirty = true;
        }

        /**
         * Check if graph needs rebuild
         */
        [[nodiscard]] bool IsDirty() const noexcept
        {
            return m_dirty;
        }

        /**
         * Get the topologically sorted execution order
         */
        [[nodiscard]] const wax::Vector<uint32_t>& ExecutionOrder() const noexcept
        {
            return m_executionOrder;
        }

        /**
         * Get root systems (no dependencies)
         */
        [[nodiscard]] const wax::Vector<uint32_t>& Roots() const noexcept
        {
            return m_roots;
        }

        /**
         * Get node by index
         */
        [[nodiscard]] SystemNode* GetNode(uint32_t index)
        {
            if (index < m_nodes.Size())
            {
                return &m_nodes[index];
            }
            return nullptr;
        }

        [[nodiscard]] const SystemNode* GetNode(uint32_t index) const
        {
            if (index < m_nodes.Size())
            {
                return &m_nodes[index];
            }
            return nullptr;
        }

        /**
         * Get dependents of a node (systems that depend on this one)
         */
        [[nodiscard]] const wax::Vector<uint32_t>* GetDependents(uint32_t index) const
        {
            if (index < m_adjacency.Size())
            {
                return &m_adjacency[index];
            }
            return nullptr;
        }

        /**
         * Get number of nodes
         */
        [[nodiscard]] size_t NodeCount() const noexcept
        {
            return m_nodes.Size();
        }

        /**
         * Check if graph has cycles (should never happen with proper construction)
         */
        [[nodiscard]] bool HasCycle() const noexcept
        {
            // If topological sort succeeded, we have all nodes in order
            return m_executionOrder.Size() != m_nodes.Size();
        }

    private:
        bool HasEdge(uint32_t from, uint32_t to) const
        {
            const auto& deps = m_adjacency[from];
            for (size_t i = 0; i < deps.Size(); ++i)
            {
                if (deps[i] == to)
                {
                    return true;
                }
            }
            return false;
        }

        void Clear()
        {
            m_nodes.Clear();
            m_adjacency.Clear();
            m_roots.Clear();
            m_executionOrder.Clear();
        }

        void ComputeTopologicalOrder()
        {
            m_executionOrder.Clear();
            m_executionOrder.Reserve(m_nodes.Size());

            // Use Kahn's algorithm
            wax::Vector<uint16_t> inDegree{m_allocator};
            inDegree.Reserve(m_nodes.Size());
            for (size_t i = 0; i < m_nodes.Size(); ++i)
            {
                inDegree.PushBack(m_nodes[i].DependencyCount());
            }

            wax::Vector<uint32_t> queue{m_allocator};
            queue.Reserve(m_nodes.Size());
            for (size_t i = 0; i < m_roots.Size(); ++i)
            {
                queue.PushBack(m_roots[i]);
            }

            size_t queueFront = 0;

            while (queueFront < queue.Size())
            {
                uint32_t current = queue[queueFront++];
                m_executionOrder.PushBack(current);

                const auto& dependents = m_adjacency[current];
                for (size_t i = 0; i < dependents.Size(); ++i)
                {
                    uint32_t dep = dependents[i];
                    if (inDegree[dep] > 0)
                    {
                        --inDegree[dep];
                        if (inDegree[dep] == 0)
                        {
                            queue.PushBack(dep);
                        }
                    }
                }
            }
        }

        wax::Vector<SystemNode> m_nodes;
        wax::Vector<wax::Vector<uint32_t>> m_adjacency;
        wax::Vector<uint32_t> m_roots;
        wax::Vector<uint32_t> m_executionOrder;
        Allocator& m_allocator;
        bool m_dirty;
    };
} // namespace queen
