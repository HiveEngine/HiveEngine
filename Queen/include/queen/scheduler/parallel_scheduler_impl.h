#pragma once

/**
 * ParallelScheduler method implementations
 *
 * This file contains implementations of ParallelScheduler methods that access World members.
 * It must be included AFTER the World class is fully defined.
 */

namespace queen
{
    template<comb::Allocator Allocator>
    void ParallelScheduler<Allocator>::RunAll(World& world, SystemStorage<Allocator>& storage)
    {
        if (graph_.IsDirty())
        {
            Build(storage);
        }

        const size_t node_count = graph_.NodeCount();
        if (node_count == 0)
        {
            return;
        }

        if (!pool_->IsRunning())
        {
            pool_->Start();
        }

        graph_.Reset();
        ResetRemainingCounts();

        Tick current_tick = world.CurrentTick();

        WaitGroup wg;
        wg.Add(static_cast<int64_t>(node_count));

        const auto& roots = graph_.Roots();
        for (size_t i = 0; i < roots.Size(); ++i)
        {
            uint32_t root_idx = roots[i];
            SubmitSystemTask(root_idx, world, storage, current_tick, wg);
        }

        wg.Wait();

        // Sync point
        world.GetCommands().FlushAll(world);
    }
}
