#include <nectar/database/dependency_graph.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/hash_set.h>

namespace nectar
{
    DependencyGraph::DependencyGraph(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_forward{alloc, 64}
        , m_reverse{alloc, 64}
    {
    }

    bool DependencyGraph::AddEdge(AssetId from, AssetId to, DepKind kind)
    {
        // Self-loop
        if (from == to)
            return false;

        if (HasEdge(from, to))
            return false;

        // Cycle check: would adding from->to create a cycle?
        // If `to` can already reach `from`, then adding from->to creates a cycle.
        if (CanReach(to, from))
            return false;

        // Insert nodes if missing
        if (!m_forward.Contains(from))
            m_forward.Insert(from, wax::Vector<DependencyEdge>{*m_alloc});
        if (!m_forward.Contains(to))
            m_forward.Insert(to, wax::Vector<DependencyEdge>{*m_alloc});
        if (!m_reverse.Contains(from))
            m_reverse.Insert(from, wax::Vector<DependencyEdge>{*m_alloc});
        if (!m_reverse.Contains(to))
            m_reverse.Insert(to, wax::Vector<DependencyEdge>{*m_alloc});

        DependencyEdge edge{from, to, kind};
        m_forward.Find(from)->PushBack(edge);
        m_reverse.Find(to)->PushBack(edge);
        return true;
    }

    bool DependencyGraph::RemoveEdge(AssetId from, AssetId to)
    {
        auto* fwd = m_forward.Find(from);
        if (!fwd)
            return false;

        bool found = false;
        for (size_t i = 0; i < fwd->Size(); ++i)
        {
            if ((*fwd)[i].m_to == to)
            {
                // Swap with last and pop
                if (i < fwd->Size() - 1)
                    (*fwd)[i] = (*fwd)[fwd->Size() - 1];
                fwd->PopBack();
                found = true;
                break;
            }
        }

        if (!found)
            return false;

        auto* rev = m_reverse.Find(to);
        if (rev)
        {
            for (size_t i = 0; i < rev->Size(); ++i)
            {
                if ((*rev)[i].m_from == from)
                {
                    if (i < rev->Size() - 1)
                        (*rev)[i] = (*rev)[rev->Size() - 1];
                    rev->PopBack();
                    break;
                }
            }
        }

        return true;
    }

    void DependencyGraph::RemoveNode(AssetId id)
    {
        // Remove all outgoing edges from forward, and their reverse entries
        auto* fwd = m_forward.Find(id);
        if (fwd)
        {
            for (size_t i = 0; i < fwd->Size(); ++i)
            {
                auto* rev = m_reverse.Find((*fwd)[i].m_to);
                if (rev)
                {
                    for (size_t j = 0; j < rev->Size(); ++j)
                    {
                        if ((*rev)[j].m_from == id)
                        {
                            if (j < rev->Size() - 1)
                                (*rev)[j] = (*rev)[rev->Size() - 1];
                            rev->PopBack();
                            break;
                        }
                    }
                }
            }
            m_forward.Remove(id);
        }

        // Remove all incoming edges from reverse, and their forward entries
        auto* rev = m_reverse.Find(id);
        if (rev)
        {
            for (size_t i = 0; i < rev->Size(); ++i)
            {
                auto* f = m_forward.Find((*rev)[i].m_from);
                if (f)
                {
                    for (size_t j = 0; j < f->Size(); ++j)
                    {
                        if ((*f)[j].m_to == id)
                        {
                            if (j < f->Size() - 1)
                                (*f)[j] = (*f)[f->Size() - 1];
                            f->PopBack();
                            break;
                        }
                    }
                }
            }
            m_reverse.Remove(id);
        }
    }

    void DependencyGraph::RemoveOutgoingEdges(AssetId id)
    {
        auto* fwd = m_forward.Find(id);
        if (!fwd)
            return;

        for (size_t i = 0; i < fwd->Size(); ++i)
        {
            auto* rev = m_reverse.Find((*fwd)[i].m_to);
            if (rev)
            {
                for (size_t j = 0; j < rev->Size(); ++j)
                {
                    if ((*rev)[j].m_from == id)
                    {
                        if (j < rev->Size() - 1)
                            (*rev)[j] = (*rev)[rev->Size() - 1];
                        rev->PopBack();
                        break;
                    }
                }
            }
        }
        fwd->Clear();
    }

    void DependencyGraph::GetDependencies(AssetId id, DepKind filter, wax::Vector<AssetId>& out) const
    {
        auto* edges = m_forward.Find(id);
        if (!edges)
            return;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if (HasFlag(filter, (*edges)[i].m_kind))
                out.PushBack((*edges)[i].m_to);
        }
    }

    void DependencyGraph::GetDependents(AssetId id, DepKind filter, wax::Vector<AssetId>& out) const
    {
        auto* edges = m_reverse.Find(id);
        if (!edges)
            return;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if (HasFlag(filter, (*edges)[i].m_kind))
                out.PushBack((*edges)[i].m_from);
        }
    }

    void DependencyGraph::GetTransitiveDependencies(AssetId id, DepKind filter, wax::Vector<AssetId>& out) const
    {
        wax::HashSet<AssetId> visited{*m_alloc};
        wax::Vector<AssetId> stack{*m_alloc};

        // Seed with direct dependencies
        auto* edges = m_forward.Find(id);
        if (!edges)
            return;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if (HasFlag(filter, (*edges)[i].m_kind))
                stack.PushBack((*edges)[i].m_to);
        }

        while (!stack.IsEmpty())
        {
            AssetId current = stack.Back();
            stack.PopBack();

            if (visited.Contains(current))
                continue;
            visited.Insert(current);
            out.PushBack(current);

            auto* next = m_forward.Find(current);
            if (!next)
                continue;
            for (size_t i = 0; i < next->Size(); ++i)
            {
                if (HasFlag(filter, (*next)[i].m_kind) && !visited.Contains((*next)[i].m_to))
                    stack.PushBack((*next)[i].m_to);
            }
        }
    }

    void DependencyGraph::GetTransitiveDependents(AssetId id, DepKind filter, wax::Vector<AssetId>& out) const
    {
        wax::HashSet<AssetId> visited{*m_alloc};
        wax::Vector<AssetId> stack{*m_alloc};

        auto* edges = m_reverse.Find(id);
        if (!edges)
            return;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if (HasFlag(filter, (*edges)[i].m_kind))
                stack.PushBack((*edges)[i].m_from);
        }

        while (!stack.IsEmpty())
        {
            AssetId current = stack.Back();
            stack.PopBack();

            if (visited.Contains(current))
                continue;
            visited.Insert(current);
            out.PushBack(current);

            auto* next = m_reverse.Find(current);
            if (!next)
                continue;
            for (size_t i = 0; i < next->Size(); ++i)
            {
                if (HasFlag(filter, (*next)[i].m_kind) && !visited.Contains((*next)[i].m_from))
                    stack.PushBack((*next)[i].m_from);
            }
        }
    }

    bool DependencyGraph::HasCycle() const
    {
        // Kahn's algorithm: if topo sort fails to include all nodes, there's a cycle
        wax::Vector<AssetId> order{*m_alloc};
        return !TopologicalSort(order);
    }

    bool DependencyGraph::TopologicalSort(wax::Vector<AssetId>& order) const
    {
        // Kahn's algorithm with in-degree counting
        wax::HashMap<AssetId, size_t> inDegree{*m_alloc};

        for (auto it = m_forward.Begin(); it != m_forward.End(); ++it)
        {
            if (!inDegree.Contains(it.Key()))
                inDegree.Insert(it.Key(), size_t{0});

            for (size_t i = 0; i < it.Value().Size(); ++i)
            {
                auto to = it.Value()[i].m_to;
                auto* deg = inDegree.Find(to);
                if (deg)
                    ++(*deg);
                else
                    inDegree.Insert(to, size_t{1});
            }
        }
        for (auto it = m_reverse.Begin(); it != m_reverse.End(); ++it)
        {
            if (!inDegree.Contains(it.Key()))
                inDegree.Insert(it.Key(), size_t{0});
        }

        wax::Vector<AssetId> queue{*m_alloc};
        for (auto it = inDegree.Begin(); it != inDegree.End(); ++it)
        {
            if (it.Value() == 0)
                queue.PushBack(it.Key());
        }

        size_t totalNodes = inDegree.Count();

        while (!queue.IsEmpty())
        {
            AssetId current = queue.Back();
            queue.PopBack();
            order.PushBack(current);

            auto* edges = m_forward.Find(current);
            if (!edges)
                continue;
            for (size_t i = 0; i < edges->Size(); ++i)
            {
                auto* deg = inDegree.Find((*edges)[i].m_to);
                if (deg && *deg > 0)
                {
                    --(*deg);
                    if (*deg == 0)
                        queue.PushBack((*edges)[i].m_to);
                }
            }
        }

        if (order.Size() != totalNodes)
            return false;

        // Reverse: Kahn's outputs dependents-first, but we want dependencies-first (build order)
        if (order.Size() < 2)
            return true;
        for (size_t i = 0, j = order.Size() - 1; i < j; ++i, --j)
        {
            AssetId tmp = order[i];
            order[i] = order[j];
            order[j] = tmp;
        }
        return true;
    }

    bool DependencyGraph::TopologicalSortLevels(wax::Vector<wax::Vector<AssetId>>& levels) const
    {
        wax::HashMap<AssetId, size_t> inDegree{*m_alloc};

        for (auto it = m_forward.Begin(); it != m_forward.End(); ++it)
        {
            if (!inDegree.Contains(it.Key()))
                inDegree.Insert(it.Key(), size_t{0});

            for (size_t i = 0; i < it.Value().Size(); ++i)
            {
                auto to = it.Value()[i].m_to;
                auto* deg = inDegree.Find(to);
                if (deg)
                    ++(*deg);
                else
                    inDegree.Insert(to, size_t{1});
            }
        }
        for (auto it = m_reverse.Begin(); it != m_reverse.End(); ++it)
        {
            if (!inDegree.Contains(it.Key()))
                inDegree.Insert(it.Key(), size_t{0});
        }

        size_t totalNodes = inDegree.Count();
        size_t processed = 0;

        // Seed first level with in-degree 0 nodes
        wax::Vector<AssetId> currentLevel{*m_alloc};
        for (auto it = inDegree.Begin(); it != inDegree.End(); ++it)
        {
            if (it.Value() == 0)
                currentLevel.PushBack(it.Key());
        }

        while (!currentLevel.IsEmpty())
        {
            levels.PushBack(static_cast<wax::Vector<AssetId>&&>(currentLevel));
            auto& justAdded = levels[levels.Size() - 1];
            processed += justAdded.Size();

            currentLevel = wax::Vector<AssetId>{*m_alloc};
            for (size_t i = 0; i < justAdded.Size(); ++i)
            {
                auto* edges = m_forward.Find(justAdded[i]);
                if (!edges)
                    continue;
                for (size_t j = 0; j < edges->Size(); ++j)
                {
                    auto* deg = inDegree.Find((*edges)[j].m_to);
                    if (deg && *deg > 0)
                    {
                        --(*deg);
                        if (*deg == 0)
                            currentLevel.PushBack((*edges)[j].m_to);
                    }
                }
            }
        }

        return processed == totalNodes;
    }

    size_t DependencyGraph::NodeCount() const
    {
        // Count unique nodes from both maps
        wax::HashSet<AssetId> nodes{*m_alloc};
        for (auto it = m_forward.Begin(); it != m_forward.End(); ++it)
            nodes.Insert(it.Key());
        for (auto it = m_reverse.Begin(); it != m_reverse.End(); ++it)
            nodes.Insert(it.Key());
        return nodes.Count();
    }

    size_t DependencyGraph::EdgeCount() const
    {
        size_t count = 0;
        for (auto it = m_forward.Begin(); it != m_forward.End(); ++it)
            count += it.Value().Size();
        return count;
    }

    bool DependencyGraph::HasNode(AssetId id) const
    {
        return m_forward.Contains(id) || m_reverse.Contains(id);
    }

    bool DependencyGraph::HasEdge(AssetId from, AssetId to) const
    {
        auto* edges = m_forward.Find(from);
        if (!edges)
            return false;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if ((*edges)[i].m_to == to)
                return true;
        }
        return false;
    }

    bool DependencyGraph::CanReach(AssetId start, AssetId target) const
    {
        wax::HashSet<AssetId> visited{*m_alloc};
        wax::Vector<AssetId> stack{*m_alloc};
        stack.PushBack(start);

        while (!stack.IsEmpty())
        {
            AssetId current = stack.Back();
            stack.PopBack();

            if (current == target)
                return true;
            if (visited.Contains(current))
                continue;
            visited.Insert(current);

            auto* edges = m_forward.Find(current);
            if (!edges)
                continue;
            for (size_t i = 0; i < edges->Size(); ++i)
            {
                if (!visited.Contains((*edges)[i].m_to))
                    stack.PushBack((*edges)[i].m_to);
            }
        }

        return false;
    }
} // namespace nectar
