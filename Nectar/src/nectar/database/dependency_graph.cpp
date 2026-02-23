#include <nectar/database/dependency_graph.h>
#include <wax/containers/hash_set.h>
#include <wax/containers/hash_map.h>

namespace nectar
{
    DependencyGraph::DependencyGraph(comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , forward_{alloc, 64}
        , reverse_{alloc, 64}
    {}

    bool DependencyGraph::AddEdge(AssetId from, AssetId to, DepKind kind)
    {
        // Self-loop
        if (from == to) return false;

        if (HasEdge(from, to)) return false;

        // Cycle check: would adding from->to create a cycle?
        // If `to` can already reach `from`, then adding from->to creates a cycle.
        if (CanReach(to, from)) return false;

        // Insert nodes if missing
        if (!forward_.Contains(from))
            forward_.Insert(from, wax::Vector<DependencyEdge>{*alloc_});
        if (!forward_.Contains(to))
            forward_.Insert(to, wax::Vector<DependencyEdge>{*alloc_});
        if (!reverse_.Contains(from))
            reverse_.Insert(from, wax::Vector<DependencyEdge>{*alloc_});
        if (!reverse_.Contains(to))
            reverse_.Insert(to, wax::Vector<DependencyEdge>{*alloc_});

        DependencyEdge edge{from, to, kind};
        forward_.Find(from)->PushBack(edge);
        reverse_.Find(to)->PushBack(edge);
        return true;
    }

    bool DependencyGraph::RemoveEdge(AssetId from, AssetId to)
    {
        auto* fwd = forward_.Find(from);
        if (!fwd) return false;

        bool found = false;
        for (size_t i = 0; i < fwd->Size(); ++i)
        {
            if ((*fwd)[i].to == to)
            {
                // Swap with last and pop
                if (i < fwd->Size() - 1)
                    (*fwd)[i] = (*fwd)[fwd->Size() - 1];
                fwd->PopBack();
                found = true;
                break;
            }
        }

        if (!found) return false;

        auto* rev = reverse_.Find(to);
        if (rev)
        {
            for (size_t i = 0; i < rev->Size(); ++i)
            {
                if ((*rev)[i].from == from)
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
        auto* fwd = forward_.Find(id);
        if (fwd)
        {
            for (size_t i = 0; i < fwd->Size(); ++i)
            {
                auto* rev = reverse_.Find((*fwd)[i].to);
                if (rev)
                {
                    for (size_t j = 0; j < rev->Size(); ++j)
                    {
                        if ((*rev)[j].from == id)
                        {
                            if (j < rev->Size() - 1)
                                (*rev)[j] = (*rev)[rev->Size() - 1];
                            rev->PopBack();
                            break;
                        }
                    }
                }
            }
            forward_.Remove(id);
        }

        // Remove all incoming edges from reverse, and their forward entries
        auto* rev = reverse_.Find(id);
        if (rev)
        {
            for (size_t i = 0; i < rev->Size(); ++i)
            {
                auto* f = forward_.Find((*rev)[i].from);
                if (f)
                {
                    for (size_t j = 0; j < f->Size(); ++j)
                    {
                        if ((*f)[j].to == id)
                        {
                            if (j < f->Size() - 1)
                                (*f)[j] = (*f)[f->Size() - 1];
                            f->PopBack();
                            break;
                        }
                    }
                }
            }
            reverse_.Remove(id);
        }
    }

    void DependencyGraph::GetDependencies(AssetId id, DepKind filter,
                                           wax::Vector<AssetId>& out) const
    {
        auto* edges = forward_.Find(id);
        if (!edges) return;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if (HasFlag(filter, (*edges)[i].kind))
                out.PushBack((*edges)[i].to);
        }
    }

    void DependencyGraph::GetDependents(AssetId id, DepKind filter,
                                         wax::Vector<AssetId>& out) const
    {
        auto* edges = reverse_.Find(id);
        if (!edges) return;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if (HasFlag(filter, (*edges)[i].kind))
                out.PushBack((*edges)[i].from);
        }
    }

    void DependencyGraph::GetTransitiveDependencies(AssetId id, DepKind filter,
                                                     wax::Vector<AssetId>& out) const
    {
        wax::HashSet<AssetId> visited{*alloc_};
        wax::Vector<AssetId> stack{*alloc_};

        // Seed with direct dependencies
        auto* edges = forward_.Find(id);
        if (!edges) return;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if (HasFlag(filter, (*edges)[i].kind))
                stack.PushBack((*edges)[i].to);
        }

        while (!stack.IsEmpty())
        {
            AssetId current = stack.Back();
            stack.PopBack();

            if (visited.Contains(current)) continue;
            visited.Insert(current);
            out.PushBack(current);

            auto* next = forward_.Find(current);
            if (!next) continue;
            for (size_t i = 0; i < next->Size(); ++i)
            {
                if (HasFlag(filter, (*next)[i].kind) && !visited.Contains((*next)[i].to))
                    stack.PushBack((*next)[i].to);
            }
        }
    }

    void DependencyGraph::GetTransitiveDependents(AssetId id, DepKind filter,
                                                   wax::Vector<AssetId>& out) const
    {
        wax::HashSet<AssetId> visited{*alloc_};
        wax::Vector<AssetId> stack{*alloc_};

        auto* edges = reverse_.Find(id);
        if (!edges) return;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if (HasFlag(filter, (*edges)[i].kind))
                stack.PushBack((*edges)[i].from);
        }

        while (!stack.IsEmpty())
        {
            AssetId current = stack.Back();
            stack.PopBack();

            if (visited.Contains(current)) continue;
            visited.Insert(current);
            out.PushBack(current);

            auto* next = reverse_.Find(current);
            if (!next) continue;
            for (size_t i = 0; i < next->Size(); ++i)
            {
                if (HasFlag(filter, (*next)[i].kind) && !visited.Contains((*next)[i].from))
                    stack.PushBack((*next)[i].from);
            }
        }
    }

    bool DependencyGraph::HasCycle() const
    {
        // Kahn's algorithm: if topo sort fails to include all nodes, there's a cycle
        wax::Vector<AssetId> order{*alloc_};
        return !TopologicalSort(order);
    }

    bool DependencyGraph::TopologicalSort(wax::Vector<AssetId>& order) const
    {
        // Kahn's algorithm with in-degree counting
        wax::HashMap<AssetId, size_t> in_degree{*alloc_};

        for (auto it = forward_.begin(); it != forward_.end(); ++it)
        {
            if (!in_degree.Contains(it.Key()))
                in_degree.Insert(it.Key(), size_t{0});

            for (size_t i = 0; i < it.Value().Size(); ++i)
            {
                auto to = it.Value()[i].to;
                auto* deg = in_degree.Find(to);
                if (deg)
                    ++(*deg);
                else
                    in_degree.Insert(to, size_t{1});
            }
        }
        for (auto it = reverse_.begin(); it != reverse_.end(); ++it)
        {
            if (!in_degree.Contains(it.Key()))
                in_degree.Insert(it.Key(), size_t{0});
        }

        wax::Vector<AssetId> queue{*alloc_};
        for (auto it = in_degree.begin(); it != in_degree.end(); ++it)
        {
            if (it.Value() == 0)
                queue.PushBack(it.Key());
        }

        size_t total_nodes = in_degree.Count();

        while (!queue.IsEmpty())
        {
            AssetId current = queue.Back();
            queue.PopBack();
            order.PushBack(current);

            auto* edges = forward_.Find(current);
            if (!edges) continue;
            for (size_t i = 0; i < edges->Size(); ++i)
            {
                auto* deg = in_degree.Find((*edges)[i].to);
                if (deg && *deg > 0)
                {
                    --(*deg);
                    if (*deg == 0)
                        queue.PushBack((*edges)[i].to);
                }
            }
        }

        if (order.Size() != total_nodes) return false;

        // Reverse: Kahn's outputs dependents-first, but we want dependencies-first (build order)
        if (order.Size() < 2) return true;
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
        wax::HashMap<AssetId, size_t> in_degree{*alloc_};

        for (auto it = forward_.begin(); it != forward_.end(); ++it)
        {
            if (!in_degree.Contains(it.Key()))
                in_degree.Insert(it.Key(), size_t{0});

            for (size_t i = 0; i < it.Value().Size(); ++i)
            {
                auto to = it.Value()[i].to;
                auto* deg = in_degree.Find(to);
                if (deg)
                    ++(*deg);
                else
                    in_degree.Insert(to, size_t{1});
            }
        }
        for (auto it = reverse_.begin(); it != reverse_.end(); ++it)
        {
            if (!in_degree.Contains(it.Key()))
                in_degree.Insert(it.Key(), size_t{0});
        }

        size_t total_nodes = in_degree.Count();
        size_t processed = 0;

        // Seed first level with in-degree 0 nodes
        wax::Vector<AssetId> current_level{*alloc_};
        for (auto it = in_degree.begin(); it != in_degree.end(); ++it)
        {
            if (it.Value() == 0)
                current_level.PushBack(it.Key());
        }

        while (!current_level.IsEmpty())
        {
            levels.PushBack(static_cast<wax::Vector<AssetId>&&>(current_level));
            auto& just_added = levels[levels.Size() - 1];
            processed += just_added.Size();

            current_level = wax::Vector<AssetId>{*alloc_};
            for (size_t i = 0; i < just_added.Size(); ++i)
            {
                auto* edges = forward_.Find(just_added[i]);
                if (!edges) continue;
                for (size_t j = 0; j < edges->Size(); ++j)
                {
                    auto* deg = in_degree.Find((*edges)[j].to);
                    if (deg && *deg > 0)
                    {
                        --(*deg);
                        if (*deg == 0)
                            current_level.PushBack((*edges)[j].to);
                    }
                }
            }
        }

        return processed == total_nodes;
    }

    size_t DependencyGraph::NodeCount() const
    {
        // Count unique nodes from both maps
        wax::HashSet<AssetId> nodes{*alloc_};
        for (auto it = forward_.begin(); it != forward_.end(); ++it)
            nodes.Insert(it.Key());
        for (auto it = reverse_.begin(); it != reverse_.end(); ++it)
            nodes.Insert(it.Key());
        return nodes.Count();
    }

    size_t DependencyGraph::EdgeCount() const
    {
        size_t count = 0;
        for (auto it = forward_.begin(); it != forward_.end(); ++it)
            count += it.Value().Size();
        return count;
    }

    bool DependencyGraph::HasNode(AssetId id) const
    {
        return forward_.Contains(id) || reverse_.Contains(id);
    }

    bool DependencyGraph::HasEdge(AssetId from, AssetId to) const
    {
        auto* edges = forward_.Find(from);
        if (!edges) return false;
        for (size_t i = 0; i < edges->Size(); ++i)
        {
            if ((*edges)[i].to == to) return true;
        }
        return false;
    }

    bool DependencyGraph::CanReach(AssetId start, AssetId target) const
    {
        wax::HashSet<AssetId> visited{*alloc_};
        wax::Vector<AssetId> stack{*alloc_};
        stack.PushBack(start);

        while (!stack.IsEmpty())
        {
            AssetId current = stack.Back();
            stack.PopBack();

            if (current == target) return true;
            if (visited.Contains(current)) continue;
            visited.Insert(current);

            auto* edges = forward_.Find(current);
            if (!edges) continue;
            for (size_t i = 0; i < edges->Size(); ++i)
            {
                if (!visited.Contains((*edges)[i].to))
                    stack.PushBack((*edges)[i].to);
            }
        }

        return false;
    }
}
