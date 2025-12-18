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
        Pending,    // Waiting for dependencies
        Ready,      // Dependencies satisfied, ready to run
        Running,    // Currently executing
        Complete    // Execution finished
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
            : system_id_{}
            , state_{SystemState::Pending}
            , dependency_count_{0}
            , unfinished_deps_{0}
        {}

        constexpr explicit SystemNode(SystemId id) noexcept
            : system_id_{id}
            , state_{SystemState::Pending}
            , dependency_count_{0}
            , unfinished_deps_{0}
        {}

        [[nodiscard]] constexpr SystemId Id() const noexcept { return system_id_; }
        [[nodiscard]] constexpr SystemState State() const noexcept { return state_; }
        [[nodiscard]] constexpr uint16_t DependencyCount() const noexcept { return dependency_count_; }
        [[nodiscard]] constexpr uint16_t UnfinishedDeps() const noexcept { return unfinished_deps_; }

        constexpr void SetState(SystemState state) noexcept { state_ = state; }
        constexpr void SetDependencyCount(uint16_t count) noexcept
        {
            dependency_count_ = count;
            unfinished_deps_ = count;
        }

        /**
         * Reset to pending state for a new frame
         */
        constexpr void Reset() noexcept
        {
            state_ = SystemState::Pending;
            unfinished_deps_ = dependency_count_;
        }

        /**
         * Decrement unfinished dependency count
         * @return true if all dependencies are now satisfied (ready to run)
         */
        constexpr bool DecrementDeps() noexcept
        {
            if (unfinished_deps_ > 0)
            {
                --unfinished_deps_;
            }
            return unfinished_deps_ == 0;
        }

        [[nodiscard]] constexpr bool IsReady() const noexcept
        {
            return state_ == SystemState::Pending && unfinished_deps_ == 0;
        }

    private:
        SystemId system_id_;
        SystemState state_;
        uint16_t dependency_count_;
        uint16_t unfinished_deps_;
    };
}
