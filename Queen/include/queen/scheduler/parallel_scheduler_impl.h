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
        // Rebuild graph if needed
        if (graph_.IsDirty())
        {
            Build(storage);
        }

        const size_t node_count = graph_.NodeCount();
        if (node_count == 0)
        {
            return;
        }

        // Ensure pool is started
        if (!pool_->IsRunning())
        {
            pool_->Start();
        }

        // Reset graph state and remaining counts
        graph_.Reset();
        ResetRemainingCounts();

        // Get current tick for change detection
        Tick current_tick = world.CurrentTick();

        // Use a wait group to track all system completions
        WaitGroup wg;
        wg.Add(static_cast<int64_t>(node_count));

        // Submit root systems first
        const auto& roots = graph_.Roots();
        for (size_t i = 0; i < roots.Size(); ++i)
        {
            uint32_t root_idx = roots[i];
            SubmitSystemTask(root_idx, world, storage, current_tick, wg);
        }

        // Wait for all systems to complete
        wg.Wait();

        // Flush all command buffers at sync point
        world.GetCommands().FlushAll(world);
    }
}
