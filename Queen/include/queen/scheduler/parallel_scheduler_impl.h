#pragma once

namespace queen
{
    template <comb::Allocator Allocator>
    void ParallelScheduler<Allocator>::RunAll(World& world, SystemStorage<Allocator>& storage)
    {
        if (m_graph.IsDirty())
        {
            Build(storage);
        }

        const size_t nodeCount = m_graph.NodeCount();
        if (nodeCount == 0)
        {
            return;
        }

        m_graph.Reset();
        ResetRemainingCounts();

        Tick currentTick = world.CurrentTick();

        drone::Counter counter{static_cast<int64_t>(nodeCount)};

        const auto& roots = m_graph.Roots();
        for (size_t i = 0; i < roots.Size(); ++i)
        {
            SubmitSystemTask(roots[i], world, storage, currentTick, counter);
        }

        counter.Wait();

        world.GetCommands().FlushAll(world);
    }
} // namespace queen
