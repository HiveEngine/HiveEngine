#pragma once

#include <hive/math/types.h>
#include <hive/math/functions.h>
#include <hive/math/geometry.h>
#include <hive/core/assert.h>
#include <wax/containers/vector.h>
#include <comb/allocator_concepts.h>
#include <comb/buddy_allocator.h>

#include <cstdint>
#include <cfloat>

namespace hive::math
{
    // 32 bytes — fits two nodes per cache line (64B)
    struct BVHNode
    {
        Float3   aabb_min;
        uint32_t left;        // left child index (internal) or first item index (leaf)
        Float3   aabb_max;
        uint32_t count;       // 0 = internal node, >0 = leaf with count items
    };
    static_assert(sizeof(BVHNode) == 32);

    // Opaque handle for dynamic insert/remove/update
    struct BVHProxy
    {
        uint32_t index{UINT32_MAX};
        [[nodiscard]] bool IsValid() const { return index != UINT32_MAX; }
    };

    namespace detail
    {
        static constexpr uint32_t kInvalidNode = UINT32_MAX;
        static constexpr uint32_t kMaxStackDepth = 64;
        static constexpr uint32_t kSAHBinCount = 8;
        static constexpr float    kFatMargin = 0.05f;
        static constexpr uint32_t kMaxLeafItems = 2;

        struct SAHBin
        {
            AABB bounds{{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
            uint32_t count{0};
        };

        [[nodiscard]] inline float SurfaceArea(const AABB& box)
        {
            float dx = box.max.x - box.min.x;
            float dy = box.max.y - box.min.y;
            float dz = box.max.z - box.min.z;
            return 2.f * (dx * dy + dy * dz + dz * dx);
        }

        [[nodiscard]] inline AABB Union(const AABB& a, const AABB& b)
        {
            return {
                {Min(a.min.x, b.min.x), Min(a.min.y, b.min.y), Min(a.min.z, b.min.z)},
                {Max(a.max.x, b.max.x), Max(a.max.y, b.max.y), Max(a.max.z, b.max.z)}
            };
        }

        [[nodiscard]] inline AABB FattenAABB(const AABB& box, float margin)
        {
            return {
                {box.min.x - margin, box.min.y - margin, box.min.z - margin},
                {box.max.x + margin, box.max.y + margin, box.max.z + margin}
            };
        }

        [[nodiscard]] inline bool Contains(const AABB& outer, const AABB& inner)
        {
            return inner.min.x >= outer.min.x && inner.min.y >= outer.min.y && inner.min.z >= outer.min.z
                && inner.max.x <= outer.max.x && inner.max.y <= outer.max.y && inner.max.z <= outer.max.z;
        }

        [[nodiscard]] inline Float3 Center(const AABB& box)
        {
            return {
                (box.min.x + box.max.x) * 0.5f,
                (box.min.y + box.max.y) * 0.5f,
                (box.min.z + box.max.z) * 0.5f
            };
        }

        [[nodiscard]] inline bool AABBOverlaps(const AABB& a, const AABB& b)
        {
            return a.min.x <= b.max.x && a.max.x >= b.min.x
                && a.min.y <= b.max.y && a.max.y >= b.min.y
                && a.min.z <= b.max.z && a.max.z >= b.min.z;
        }

        // Ray-AABB slab test
        [[nodiscard]] inline bool RayAABB(Float3 origin, Float3 inv_dir, float max_t, const AABB& box)
        {
            float t1 = (box.min.x - origin.x) * inv_dir.x;
            float t2 = (box.max.x - origin.x) * inv_dir.x;
            float tmin = Min(t1, t2);
            float tmax = Max(t1, t2);

            t1 = (box.min.y - origin.y) * inv_dir.y;
            t2 = (box.max.y - origin.y) * inv_dir.y;
            tmin = Max(tmin, Min(t1, t2));
            tmax = Min(tmax, Max(t1, t2));

            t1 = (box.min.z - origin.z) * inv_dir.z;
            t2 = (box.max.z - origin.z) * inv_dir.z;
            tmin = Max(tmin, Min(t1, t2));
            tmax = Min(tmax, Max(t1, t2));

            return tmax >= Max(tmin, 0.f) && tmin < max_t;
        }
    }

    template<comb::Allocator Allocator>
    class BVH
    {
    public:
        explicit BVH(Allocator& alloc)
            : nodes_{alloc}
            , right_{alloc}
            , parent_{alloc}
            , items_{alloc}
            , item_aabbs_{alloc}
            , fat_aabbs_{alloc}
            , free_list_{alloc}
        {}

        // Build from array of AABBs (binned SAH top-down). Replaces existing content.
        void Build(const AABB* aabbs, uint32_t count)
        {
            Clear();
            item_count_ = count;

            if (count == 0) return;

            items_.Resize(count);
            item_aabbs_.Resize(count);
            fat_aabbs_.Resize(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                items_[i] = i;
                item_aabbs_[i] = aabbs[i];
                fat_aabbs_[i] = detail::FattenAABB(aabbs[i], detail::kFatMargin);
            }

            nodes_.Reserve(count * 2);
            right_.Reserve(count * 2);
            parent_.Reserve(count * 2);

            root_ = AllocNode();
            nodes_[root_].left = 0;
            nodes_[root_].count = count;
            parent_[root_] = detail::kInvalidNode;

            UpdateLeafBounds(root_);
            Subdivide(root_);
        }

        // Insert single AABB dynamically
        [[nodiscard]] BVHProxy Insert(const AABB& aabb)
        {
            uint32_t item_idx = item_count_++;
            items_.PushBack(item_idx);
            item_aabbs_.PushBack(aabb);
            fat_aabbs_.PushBack(detail::FattenAABB(aabb, detail::kFatMargin));

            uint32_t leaf = AllocNode();
            nodes_[leaf].aabb_min = fat_aabbs_[item_idx].min;
            nodes_[leaf].aabb_max = fat_aabbs_[item_idx].max;
            nodes_[leaf].left = item_idx;
            nodes_[leaf].count = 1;

            if (root_ == detail::kInvalidNode)
            {
                root_ = leaf;
                parent_[leaf] = detail::kInvalidNode;
            }
            else
            {
                InsertLeaf(leaf);
            }

            return BVHProxy{leaf};
        }

        // Remove a previously inserted proxy
        void Remove(BVHProxy proxy)
        {
            hive::Assert(proxy.IsValid(), "Invalid BVH proxy");
            RemoveLeaf(proxy.index);
            FreeNode(proxy.index);
        }

        // Update AABB. Returns true if the BVH was restructured (fat AABB exceeded).
        bool Update(BVHProxy proxy, const AABB& new_aabb)
        {
            hive::Assert(proxy.IsValid(), "Invalid BVH proxy");
            auto& leaf = nodes_[proxy.index];
            hive::Assert(leaf.count > 0, "Proxy must be a leaf");

            uint32_t item_idx = leaf.left;
            item_aabbs_[item_idx] = new_aabb;

            AABB current_fat{leaf.aabb_min, leaf.aabb_max};
            if (detail::Contains(current_fat, new_aabb))
                return false;

            RemoveLeaf(proxy.index);
            fat_aabbs_[item_idx] = detail::FattenAABB(new_aabb, detail::kFatMargin);
            leaf.aabb_min = fat_aabbs_[item_idx].min;
            leaf.aabb_max = fat_aabbs_[item_idx].max;

            if (root_ == detail::kInvalidNode)
            {
                root_ = proxy.index;
                parent_[proxy.index] = detail::kInvalidNode;
            }
            else
            {
                InsertLeaf(proxy.index);
            }
            return true;
        }

        // Frustum culling — calls cb(uint32_t item_index) for each visible item
        template<typename Callback>
        void QueryFrustum(const Frustum& frustum, Callback&& cb) const
        {
            if (root_ == detail::kInvalidNode) return;

            uint32_t stack[detail::kMaxStackDepth];
            uint32_t sp = 0;
            stack[sp++] = root_;

            while (sp > 0)
            {
                uint32_t ni = stack[--sp];
                const auto& n = nodes_[ni];
                AABB box{n.aabb_min, n.aabb_max};

                if (!IsVisible(frustum, box))
                    continue;

                if (n.count > 0)
                {
                    for (uint32_t i = 0; i < n.count; ++i)
                    {
                        uint32_t item = items_[n.left + i];
                        if (IsVisible(frustum, item_aabbs_[item]))
                            cb(item);
                    }
                }
                else
                {
                    hive::Assert(sp + 2 <= detail::kMaxStackDepth, "BVH stack overflow");
                    stack[sp++] = n.left;
                    stack[sp++] = right_[ni];
                }
            }
        }

        // Raycast — calls cb(uint32_t item_index) for each AABB hit
        template<typename Callback>
        void QueryRay(Float3 origin, Float3 direction, float max_t, Callback&& cb) const
        {
            if (root_ == detail::kInvalidNode) return;

            Float3 inv_dir{
                Abs(direction.x) > kEpsilon ? 1.f / direction.x : (direction.x >= 0.f ? FLT_MAX : -FLT_MAX),
                Abs(direction.y) > kEpsilon ? 1.f / direction.y : (direction.y >= 0.f ? FLT_MAX : -FLT_MAX),
                Abs(direction.z) > kEpsilon ? 1.f / direction.z : (direction.z >= 0.f ? FLT_MAX : -FLT_MAX)
            };

            uint32_t stack[detail::kMaxStackDepth];
            uint32_t sp = 0;
            stack[sp++] = root_;

            while (sp > 0)
            {
                uint32_t ni = stack[--sp];
                const auto& n = nodes_[ni];
                AABB box{n.aabb_min, n.aabb_max};

                if (!detail::RayAABB(origin, inv_dir, max_t, box))
                    continue;

                if (n.count > 0)
                {
                    for (uint32_t i = 0; i < n.count; ++i)
                    {
                        uint32_t item = items_[n.left + i];
                        if (detail::RayAABB(origin, inv_dir, max_t, item_aabbs_[item]))
                            cb(item);
                    }
                }
                else
                {
                    hive::Assert(sp + 2 <= detail::kMaxStackDepth, "BVH stack overflow");
                    stack[sp++] = n.left;
                    stack[sp++] = right_[ni];
                }
            }
        }

        // AABB overlap — calls cb(uint32_t item_index) for each overlapping item
        template<typename Callback>
        void QueryAABB(const AABB& query, Callback&& cb) const
        {
            if (root_ == detail::kInvalidNode) return;

            uint32_t stack[detail::kMaxStackDepth];
            uint32_t sp = 0;
            stack[sp++] = root_;

            while (sp > 0)
            {
                uint32_t ni = stack[--sp];
                const auto& n = nodes_[ni];
                AABB box{n.aabb_min, n.aabb_max};

                if (!detail::AABBOverlaps(query, box))
                    continue;

                if (n.count > 0)
                {
                    for (uint32_t i = 0; i < n.count; ++i)
                    {
                        uint32_t item = items_[n.left + i];
                        if (detail::AABBOverlaps(query, item_aabbs_[item]))
                            cb(item);
                    }
                }
                else
                {
                    hive::Assert(sp + 2 <= detail::kMaxStackDepth, "BVH stack overflow");
                    stack[sp++] = n.left;
                    stack[sp++] = right_[ni];
                }
            }
        }

        // Refit all internal node bounds bottom-up (after external item_aabb modifications)
        void Refit()
        {
            if (root_ == detail::kInvalidNode) return;
            RefitNode(root_);
        }

        void Clear()
        {
            nodes_.Clear();
            right_.Clear();
            parent_.Clear();
            items_.Clear();
            item_aabbs_.Clear();
            fat_aabbs_.Clear();
            free_list_.Clear();
            root_ = detail::kInvalidNode;
            item_count_ = 0;
        }

        [[nodiscard]] uint32_t NodeCount() const
        {
            return static_cast<uint32_t>(nodes_.Size()) - static_cast<uint32_t>(free_list_.Size());
        }
        [[nodiscard]] uint32_t ItemCount() const { return item_count_; }
        [[nodiscard]] bool IsEmpty() const { return root_ == detail::kInvalidNode; }

        // Direct item AABB access (modify then call Refit)
        AABB& ItemAABB(uint32_t idx) { return item_aabbs_[idx]; }
        const AABB& ItemAABB(uint32_t idx) const { return item_aabbs_[idx]; }

    private:
        uint32_t AllocNode()
        {
            if (!free_list_.IsEmpty())
            {
                uint32_t idx = free_list_.Back();
                free_list_.PopBack();
                nodes_[idx] = BVHNode{};
                right_[idx] = detail::kInvalidNode;
                parent_[idx] = detail::kInvalidNode;
                return idx;
            }
            uint32_t idx = static_cast<uint32_t>(nodes_.Size());
            nodes_.PushBack(BVHNode{});
            right_.PushBack(detail::kInvalidNode);
            parent_.PushBack(detail::kInvalidNode);
            return idx;
        }

        void FreeNode(uint32_t idx) { free_list_.PushBack(idx); }

        void UpdateLeafBounds(uint32_t ni)
        {
            auto& n = nodes_[ni];
            hive::Assert(n.count > 0, "UpdateLeafBounds called on internal node");
            n.aabb_min = {FLT_MAX, FLT_MAX, FLT_MAX};
            n.aabb_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
            for (uint32_t i = 0; i < n.count; ++i)
            {
                const auto& aabb = item_aabbs_[items_[n.left + i]];
                n.aabb_min.x = Min(n.aabb_min.x, aabb.min.x);
                n.aabb_min.y = Min(n.aabb_min.y, aabb.min.y);
                n.aabb_min.z = Min(n.aabb_min.z, aabb.min.z);
                n.aabb_max.x = Max(n.aabb_max.x, aabb.max.x);
                n.aabb_max.y = Max(n.aabb_max.y, aabb.max.y);
                n.aabb_max.z = Max(n.aabb_max.z, aabb.max.z);
            }
        }

        void Subdivide(uint32_t node_idx)
        {
            auto& node = nodes_[node_idx];
            if (node.count <= detail::kMaxLeafItems)
                return;

            // Centroid bounds for binning
            AABB cb{{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
            for (uint32_t i = 0; i < node.count; ++i)
            {
                Float3 c = detail::Center(item_aabbs_[items_[node.left + i]]);
                cb.min.x = Min(cb.min.x, c.x); cb.min.y = Min(cb.min.y, c.y); cb.min.z = Min(cb.min.z, c.z);
                cb.max.x = Max(cb.max.x, c.x); cb.max.y = Max(cb.max.y, c.y); cb.max.z = Max(cb.max.z, c.z);
            }

            float ext[3] = {cb.max.x - cb.min.x, cb.max.y - cb.min.y, cb.max.z - cb.min.z};
            float cmin[3] = {cb.min.x, cb.min.y, cb.min.z};

            float best_cost = FLT_MAX;
            int best_axis = -1;
            uint32_t best_split = 0;

            for (int axis = 0; axis < 3; ++axis)
            {
                if (ext[axis] < kEpsilon) continue;

                detail::SAHBin bins[detail::kSAHBinCount]{};
                float inv_ext = 1.f / ext[axis];

                for (uint32_t i = 0; i < node.count; ++i)
                {
                    uint32_t item = items_[node.left + i];
                    Float3 c = detail::Center(item_aabbs_[item]);
                    float pos[3] = {c.x, c.y, c.z};
                    uint32_t bin = static_cast<uint32_t>((pos[axis] - cmin[axis]) * inv_ext * detail::kSAHBinCount);
                    if (bin >= detail::kSAHBinCount) bin = detail::kSAHBinCount - 1;
                    bins[bin].count++;
                    bins[bin].bounds = detail::Union(bins[bin].bounds, item_aabbs_[item]);
                }

                // Left sweep
                float left_area[detail::kSAHBinCount - 1];
                uint32_t left_cnt[detail::kSAHBinCount - 1];
                {
                    AABB acc{{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
                    uint32_t cnt = 0;
                    for (uint32_t s = 0; s < detail::kSAHBinCount - 1; ++s)
                    {
                        acc = detail::Union(acc, bins[s].bounds);
                        cnt += bins[s].count;
                        left_area[s] = detail::SurfaceArea(acc);
                        left_cnt[s] = cnt;
                    }
                }

                // Right sweep
                float right_area[detail::kSAHBinCount - 1];
                uint32_t right_cnt[detail::kSAHBinCount - 1];
                {
                    AABB acc{{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
                    uint32_t cnt = 0;
                    for (uint32_t s = detail::kSAHBinCount - 1; s > 0; --s)
                    {
                        acc = detail::Union(acc, bins[s].bounds);
                        cnt += bins[s].count;
                        right_area[s - 1] = detail::SurfaceArea(acc);
                        right_cnt[s - 1] = cnt;
                    }
                }

                for (uint32_t s = 0; s < detail::kSAHBinCount - 1; ++s)
                {
                    if (left_cnt[s] == 0 || right_cnt[s] == 0) continue;
                    float cost = left_area[s] * static_cast<float>(left_cnt[s])
                               + right_area[s] * static_cast<float>(right_cnt[s]);
                    if (cost < best_cost)
                    {
                        best_cost = cost;
                        best_axis = axis;
                        best_split = s;
                    }
                }
            }

            if (best_axis < 0) return;

            float leaf_cost = detail::SurfaceArea(AABB{node.aabb_min, node.aabb_max}) * static_cast<float>(node.count);
            if (best_cost >= leaf_cost) return;

            // Partition items in-place
            float split_pos = cmin[best_axis]
                + (static_cast<float>(best_split + 1) / detail::kSAHBinCount) * ext[best_axis];

            uint32_t first = node.left;
            uint32_t last = first + node.count;
            uint32_t mid = first;
            for (uint32_t i = first; i < last; ++i)
            {
                Float3 c = detail::Center(item_aabbs_[items_[i]]);
                float pos[3] = {c.x, c.y, c.z};
                if (pos[best_axis] < split_pos)
                {
                    uint32_t tmp = items_[i];
                    items_[i] = items_[mid];
                    items_[mid] = tmp;
                    mid++;
                }
            }

            // Degenerate — force half split
            if (mid == first || mid == last)
                mid = first + node.count / 2;

            uint32_t lcount = mid - first;
            uint32_t rcount = node.count - lcount;

            uint32_t lc = AllocNode();
            uint32_t rc = AllocNode();

            // node ref possibly invalidated by AllocNode — use node_idx
            nodes_[lc].left = first;
            nodes_[lc].count = lcount;
            parent_[lc] = node_idx;

            nodes_[rc].left = mid;
            nodes_[rc].count = rcount;
            parent_[rc] = node_idx;

            nodes_[node_idx].left = lc;
            nodes_[node_idx].count = 0;
            right_[node_idx] = rc;

            UpdateLeafBounds(lc);
            UpdateLeafBounds(rc);
            Subdivide(lc);
            Subdivide(rc);
        }

        // Catto-style branch-and-bound insertion
        void InsertLeaf(uint32_t leaf_idx)
        {
            AABB leaf_box{nodes_[leaf_idx].aabb_min, nodes_[leaf_idx].aabb_max};

            // Find best sibling
            uint32_t best_sibling = root_;
            float best_cost = detail::SurfaceArea(
                detail::Union(AABB{nodes_[root_].aabb_min, nodes_[root_].aabb_max}, leaf_box));

            struct Entry { uint32_t node; float inherited; };
            Entry stack[detail::kMaxStackDepth];
            uint32_t sp = 0;
            stack[sp++] = {root_, 0.f};

            while (sp > 0)
            {
                auto [ni, inherited] = stack[--sp];
                const auto& n = nodes_[ni];
                AABB nbox{n.aabb_min, n.aabb_max};
                AABB combined = detail::Union(nbox, leaf_box);
                float direct = detail::SurfaceArea(combined);
                float total = direct + inherited;

                if (total < best_cost)
                {
                    best_cost = total;
                    best_sibling = ni;
                }

                float delta = direct - detail::SurfaceArea(nbox);
                float lower = detail::SurfaceArea(leaf_box) + inherited + delta;
                if (lower >= best_cost) continue;

                if (n.count == 0)
                {
                    float child_inh = inherited + delta;
                    hive::Assert(sp + 2 <= detail::kMaxStackDepth, "BVH insert stack overflow");
                    stack[sp++] = {n.left, child_inh};
                    stack[sp++] = {right_[ni], child_inh};
                }
            }

            // Create new internal parent between sibling and leaf
            uint32_t old_parent = parent_[best_sibling];
            uint32_t new_parent = AllocNode();

            AABB sbox{nodes_[best_sibling].aabb_min, nodes_[best_sibling].aabb_max};
            AABB combined = detail::Union(sbox, leaf_box);
            nodes_[new_parent].aabb_min = combined.min;
            nodes_[new_parent].aabb_max = combined.max;
            nodes_[new_parent].left = best_sibling;
            nodes_[new_parent].count = 0;
            right_[new_parent] = leaf_idx;
            parent_[new_parent] = old_parent;

            if (old_parent != detail::kInvalidNode)
            {
                if (nodes_[old_parent].left == best_sibling)
                    nodes_[old_parent].left = new_parent;
                else
                    right_[old_parent] = new_parent;
            }
            else
            {
                root_ = new_parent;
            }

            parent_[best_sibling] = new_parent;
            parent_[leaf_idx] = new_parent;

            RefitAncestors(new_parent);
        }

        void RemoveLeaf(uint32_t leaf_idx)
        {
            if (leaf_idx == root_)
            {
                root_ = detail::kInvalidNode;
                return;
            }

            uint32_t par = parent_[leaf_idx];
            hive::Assert(par != detail::kInvalidNode, "Leaf has no parent");

            uint32_t sibling = (nodes_[par].left == leaf_idx) ? right_[par] : nodes_[par].left;
            uint32_t gp = parent_[par];

            if (gp != detail::kInvalidNode)
            {
                if (nodes_[gp].left == par)
                    nodes_[gp].left = sibling;
                else
                    right_[gp] = sibling;

                parent_[sibling] = gp;
                FreeNode(par);
                RefitAncestors(gp);
            }
            else
            {
                root_ = sibling;
                parent_[sibling] = detail::kInvalidNode;
                FreeNode(par);
            }
        }

        void RefitAncestors(uint32_t ni)
        {
            uint32_t idx = ni;
            while (idx != detail::kInvalidNode)
            {
                auto& n = nodes_[idx];
                if (n.count == 0)
                {
                    const auto& l = nodes_[n.left];
                    const auto& r = nodes_[right_[idx]];
                    n.aabb_min = {Min(l.aabb_min.x, r.aabb_min.x),
                                  Min(l.aabb_min.y, r.aabb_min.y),
                                  Min(l.aabb_min.z, r.aabb_min.z)};
                    n.aabb_max = {Max(l.aabb_max.x, r.aabb_max.x),
                                  Max(l.aabb_max.y, r.aabb_max.y),
                                  Max(l.aabb_max.z, r.aabb_max.z)};
                }
                idx = parent_[idx];
            }
        }

        AABB RefitNode(uint32_t ni)
        {
            auto& n = nodes_[ni];
            if (n.count > 0)
            {
                // Leaf — recompute from items
                n.aabb_min = {FLT_MAX, FLT_MAX, FLT_MAX};
                n.aabb_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
                for (uint32_t i = 0; i < n.count; ++i)
                {
                    const auto& aabb = item_aabbs_[items_[n.left + i]];
                    n.aabb_min.x = Min(n.aabb_min.x, aabb.min.x);
                    n.aabb_min.y = Min(n.aabb_min.y, aabb.min.y);
                    n.aabb_min.z = Min(n.aabb_min.z, aabb.min.z);
                    n.aabb_max.x = Max(n.aabb_max.x, aabb.max.x);
                    n.aabb_max.y = Max(n.aabb_max.y, aabb.max.y);
                    n.aabb_max.z = Max(n.aabb_max.z, aabb.max.z);
                }
                return {n.aabb_min, n.aabb_max};
            }

            AABB lb = RefitNode(n.left);
            AABB rb = RefitNode(right_[ni]);
            AABB combined = detail::Union(lb, rb);
            n.aabb_min = combined.min;
            n.aabb_max = combined.max;
            return combined;
        }

        wax::Vector<BVHNode, Allocator>  nodes_;
        wax::Vector<uint32_t, Allocator> right_;   // right child index (parallel to nodes_)
        wax::Vector<uint32_t, Allocator> parent_;   // parent index (parallel to nodes_)
        wax::Vector<uint32_t, Allocator> items_;    // item indices, partitioned by Build
        wax::Vector<AABB, Allocator>     item_aabbs_;
        wax::Vector<AABB, Allocator>     fat_aabbs_;
        wax::Vector<uint32_t, Allocator> free_list_;

        uint32_t root_{detail::kInvalidNode};
        uint32_t item_count_{0};
    };

    using BuddyBVH = BVH<comb::BuddyAllocator>;
}
