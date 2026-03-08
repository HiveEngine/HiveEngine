#pragma once

/**
 * ParallelScheduler method implementations
 *
 * This file contains implementations of ParallelScheduler methods that access World members.
 * It must be included AFTER the World class is fully defined.
 */

namespace queen
{
    template <comb::Allocator Allocator>
    void ParallelScheduler<Allocator>::RunAll(World& world, SystemStorage<Allocator>& storage) {
        if (m_graph.IsDirty())
        {
            Build(storage);
        }

        const size_t nodeCount = m_graph.NodeCount();
        if (nodeCount == 0)
        {
            return;
        }

        if (!m_pool->IsRunning())
        {
            m_pool->Start();
        }

        m_graph.Reset();
        ResetRemainingCounts();

        Tick currentTick = world.CurrentTick();

        WaitGroup wg;
        wg.Add(static_cast<int64_t>(nodeCount));

        const auto& roots = m_graph.Roots();
        for (size_t i = 0; i < roots.Size(); ++i)
        {
            uint32_t rootIdx = roots[i];
            SubmitSystemTask(rootIdx, world, storage, currentTick, wg);
        }

        wg.Wait();

        // Sync point
        world.GetCommands().FlushAll(world);
    }
} // namespace queen
