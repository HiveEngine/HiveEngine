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
    template<comb::Allocator Allocator>
    void Scheduler<Allocator>::RunAll(World& world, SystemStorage<Allocator>& storage)
    {
        HIVE_PROFILE_SCOPE_N("Scheduler::RunAll");

        if (graph_.IsDirty())
        {
            graph_.Build(storage);
        }

        graph_.Reset();

        Tick current_tick = world.CurrentTick();

        const auto& order = graph_.ExecutionOrder();
        for (size_t i = 0; i < order.Size(); ++i)
        {
            uint32_t node_index = order[i];
            SystemNode* node = graph_.GetNode(node_index);

            if (node != nullptr)
            {
                node->SetState(SystemState::Running);

                SystemDescriptor<Allocator>* system = storage.GetSystemByIndex(node_index);
                if (system != nullptr)
                {
                    HIVE_PROFILE_SCOPE_N("ExecuteSystem");
                    HIVE_PROFILE_ZONE_NAME(system->Name(), std::strlen(system->Name()));
                    system->Execute(world, current_tick);
                }

                node->SetState(SystemState::Complete);
            }
        }

        // Sync point
        world.GetCommands().FlushAll(world);
    }
}
