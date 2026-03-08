#pragma once

#include <queen/system/system_id.h>

#include <cstdint>

namespace queen
{
    /**
     * Execution state of a system in the scheduler
     */
    enum class SystemState : uint8_t
    {
        PENDING, // Waiting for dependencies
        READY,   // Dependencies satisfied, ready to run
        RUNNING, // Currently executing
        COMPLETE // Execution finished
    };

    /**
     * A node in the system dependency graph
     *
     * SystemNode represents a system and its relationships to other systems
     * in the dependency graph. It tracks which systems must run before this one
     * (dependencies) and which systems are waiting for this one (dependents).
     *
     * Use cases:
     * - Building execution order for systems
     * - Detecting parallel execution opportunities
     * - Managing system completion notifications
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ system_id_: SystemId                                            │
     * │ state_: SystemState                                             │
     * │ dependency_count_: uint16_t (original count)                    │
     * │ unfinished_deps_: uint16_t (runtime countdown)                  │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Note: Dependencies and dependents are stored externally in the graph
     * using adjacency lists to allow dynamic sizing with allocators.
     */
    class SystemNode
    {
    public:
        constexpr SystemNode() noexcept
            : m_systemId{}
            , m_state{SystemState::PENDING}
            , m_dependencyCount{0}
            , m_unfinishedDeps{0}
        {
        }

        constexpr explicit SystemNode(SystemId id) noexcept
            : m_systemId{id}
            , m_state{SystemState::PENDING}
            , m_dependencyCount{0}
            , m_unfinishedDeps{0}
        {
        }

        [[nodiscard]] constexpr SystemId Id() const noexcept
        {
            return m_systemId;
        }
        [[nodiscard]] constexpr SystemState State() const noexcept
        {
            return m_state;
        }
        [[nodiscard]] constexpr uint16_t DependencyCount() const noexcept
        {
            return m_dependencyCount;
        }
        [[nodiscard]] constexpr uint16_t UnfinishedDeps() const noexcept
        {
            return m_unfinishedDeps;
        }

        constexpr void SetState(SystemState state) noexcept
        {
            m_state = state;
        }
        constexpr void SetDependencyCount(uint16_t count) noexcept
        {
            m_dependencyCount = count;
            m_unfinishedDeps = count;
        }

        constexpr void IncrementDependencyCount() noexcept
        {
            ++m_dependencyCount;
            ++m_unfinishedDeps;
        }

        /**
         * Reset to pending state for a new frame
         */
        constexpr void Reset() noexcept
        {
            m_state = SystemState::PENDING;
            m_unfinishedDeps = m_dependencyCount;
        }

        /**
         * Decrement unfinished dependency count
         * @return true if all dependencies are now satisfied (ready to run)
         */
        constexpr bool DecrementDeps() noexcept
        {
            if (m_unfinishedDeps > 0)
            {
                --m_unfinishedDeps;
            }
            return m_unfinishedDeps == 0;
        }

        [[nodiscard]] constexpr bool IsReady() const noexcept
        {
            return m_state == SystemState::PENDING && m_unfinishedDeps == 0;
        }

    private:
        SystemId m_systemId;
        SystemState m_state;
        uint16_t m_dependencyCount;
        uint16_t m_unfinishedDeps;
    };
} // namespace queen
