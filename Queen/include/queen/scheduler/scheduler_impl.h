#pragma once

/**
 * Scheduler method implementations
 *
 * This file contains implementations of Scheduler methods that access World members.
 * It must be included AFTER the World class is fully defined.
 */

#include <hive/profiling/profiler.h>

namespace queen
{
    template <comb::Allocator Allocator>
    void Scheduler<Allocator>::RunAll(World& world, SystemStorage<Allocator>& storage) {
        HIVE_PROFILE_SCOPE_N("Scheduler::RunAll");

        if (m_graph.IsDirty())
        {
            m_graph.Build(storage);
        }

        m_graph.Reset();

        Tick currentTick = world.CurrentTick();

        const auto& order = m_graph.ExecutionOrder();
        for (size_t i = 0; i < order.Size(); ++i)
        {
            uint32_t nodeIndex = order[i];
            SystemNode* node = m_graph.GetNode(nodeIndex);

            if (node != nullptr)
            {
                node->SetState(SystemState::RUNNING);

                SystemDescriptor<Allocator>* system = storage.GetSystemByIndex(nodeIndex);
                if (system != nullptr)
                {
                    HIVE_PROFILE_SCOPE_N("ExecuteSystem");
                    HIVE_PROFILE_ZONE_NAME(system->Name(), std::strlen(system->Name()));
                    system->Execute(world, currentTick);
                }

                node->SetState(SystemState::COMPLETE);
            }
        }

        // Sync point
        world.GetCommands().FlushAll(world);
    }
} // namespace queen
