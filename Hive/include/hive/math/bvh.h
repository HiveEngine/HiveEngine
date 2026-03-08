#pragma once

#include <hive/core/assert.h>
#include <hive/math/functions.h>
#include <hive/math/geometry.h>
#include <hive/math/types.h>

#include <comb/allocator_concepts.h>
#include <comb/buddy_allocator.h>

#include <wax/containers/vector.h>

#include <cfloat>
#include <cstdint>

namespace hive::math
{
    struct BVHNode
    {
        Float3 m_aabbMin;
        uint32_t m_left;
        Float3 m_aabbMax;
        uint32_t m_count;
    };
    static_assert(sizeof(BVHNode) == 32);

    struct BVHProxy
    {
        uint32_t m_index{UINT32_MAX};

        [[nodiscard]] bool IsValid() const { return m_index != UINT32_MAX; }
    };

    namespace detail
    {
        inline constexpr uint32_t kInvalidNode = UINT32_MAX;
        inline constexpr uint32_t kMaxStackDepth = 64;
        inline constexpr uint32_t kSAHBinCount = 8;
        inline constexpr float kFatMargin = 0.05f;
        inline constexpr uint32_t kMaxLeafItems = 2;

        struct SAHBin
        {
            AABB m_bounds{{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
            uint32_t m_count{0};
        };

        [[nodiscard]] inline float SurfaceArea(const AABB& box) {
            const float dx = box.m_max.m_x - box.m_min.m_x;
            const float dy = box.m_max.m_y - box.m_min.m_y;
            const float dz = box.m_max.m_z - box.m_min.m_z;
            return 2.f * (dx * dy + dy * dz + dz * dx);
        }

        [[nodiscard]] inline AABB Union(const AABB& a, const AABB& b) {
            return {{Min(a.m_min.m_x, b.m_min.m_x), Min(a.m_min.m_y, b.m_min.m_y), Min(a.m_min.m_z, b.m_min.m_z)},
                    {Max(a.m_max.m_x, b.m_max.m_x), Max(a.m_max.m_y, b.m_max.m_y), Max(a.m_max.m_z, b.m_max.m_z)}};
        }

        [[nodiscard]] inline AABB FattenAABB(const AABB& box, float margin) {
            return {{box.m_min.m_x - margin, box.m_min.m_y - margin, box.m_min.m_z - margin},
                    {box.m_max.m_x + margin, box.m_max.m_y + margin, box.m_max.m_z + margin}};
        }

        [[nodiscard]] inline bool Contains(const AABB& outer, const AABB& inner) {
            return inner.m_min.m_x >= outer.m_min.m_x && inner.m_min.m_y >= outer.m_min.m_y &&
                   inner.m_min.m_z >= outer.m_min.m_z && inner.m_max.m_x <= outer.m_max.m_x &&
                   inner.m_max.m_y <= outer.m_max.m_y && inner.m_max.m_z <= outer.m_max.m_z;
        }

        [[nodiscard]] inline Float3 Center(const AABB& box) {
            return {(box.m_min.m_x + box.m_max.m_x) * 0.5f, (box.m_min.m_y + box.m_max.m_y) * 0.5f,
                    (box.m_min.m_z + box.m_max.m_z) * 0.5f};
        }

        [[nodiscard]] inline bool AABBOverlaps(const AABB& a, const AABB& b) {
            return a.m_min.m_x <= b.m_max.m_x && a.m_max.m_x >= b.m_min.m_x && a.m_min.m_y <= b.m_max.m_y &&
                   a.m_max.m_y >= b.m_min.m_y && a.m_min.m_z <= b.m_max.m_z && a.m_max.m_z >= b.m_min.m_z;
        }

        [[nodiscard]] inline bool RayAABB(Float3 origin, Float3 invDir, float maxT, const AABB& box) {
            float t1 = (box.m_min.m_x - origin.m_x) * invDir.m_x;
            float t2 = (box.m_max.m_x - origin.m_x) * invDir.m_x;
            float tMin = Min(t1, t2);
            float tMax = Max(t1, t2);

            t1 = (box.m_min.m_y - origin.m_y) * invDir.m_y;
            t2 = (box.m_max.m_y - origin.m_y) * invDir.m_y;
            tMin = Max(tMin, Min(t1, t2));
            tMax = Min(tMax, Max(t1, t2));

            t1 = (box.m_min.m_z - origin.m_z) * invDir.m_z;
            t2 = (box.m_max.m_z - origin.m_z) * invDir.m_z;
            tMin = Max(tMin, Min(t1, t2));
            tMax = Min(tMax, Max(t1, t2));

            return tMax >= Max(tMin, 0.f) && tMin < maxT;
        }
    } // namespace detail

    template <comb::Allocator Allocator> class BVH
    {
    public:
        explicit BVH(Allocator& alloc)
            : m_nodes{alloc}
            , m_right{alloc}
            , m_parent{alloc}
            , m_items{alloc}
            , m_itemAabbs{alloc}
            , m_fatAabbs{alloc}
            , m_freeList{alloc} {}

        void Build(const AABB* aabbs, uint32_t count) {
            Clear();
            m_itemCount = count;

            if (count == 0)
            {
                return;
            }

            m_items.Resize(count);
            m_itemAabbs.Resize(count);
            m_fatAabbs.Resize(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                m_items[i] = i;
                m_itemAabbs[i] = aabbs[i];
                m_fatAabbs[i] = detail::FattenAABB(aabbs[i], detail::kFatMargin);
            }

            m_nodes.Reserve(count * 2);
            m_right.Reserve(count * 2);
            m_parent.Reserve(count * 2);

            m_root = AllocNode();
            m_nodes[m_root].m_left = 0;
            m_nodes[m_root].m_count = count;
            m_parent[m_root] = detail::kInvalidNode;

            UpdateLeafBounds(m_root);
            Subdivide(m_root);
        }

        [[nodiscard]] BVHProxy Insert(const AABB& aabb) {
            const uint32_t itemIndex = m_itemCount++;
            m_items.PushBack(itemIndex);
            m_itemAabbs.PushBack(aabb);
            m_fatAabbs.PushBack(detail::FattenAABB(aabb, detail::kFatMargin));

            const uint32_t leaf = AllocNode();
            const AABB& fatAabb = m_fatAabbs[itemIndex];
            m_nodes[leaf].m_aabbMin = fatAabb.m_min;
            m_nodes[leaf].m_aabbMax = fatAabb.m_max;
            m_nodes[leaf].m_left = itemIndex;
            m_nodes[leaf].m_count = 1;

            if (m_root == detail::kInvalidNode)
            {
                m_root = leaf;
                m_parent[leaf] = detail::kInvalidNode;
            }
            else
            {
                InsertLeaf(leaf);
            }

            return BVHProxy{leaf};
        }

        void Remove(BVHProxy proxy) {
            hive::Assert(proxy.IsValid(), "Invalid BVH proxy");
            RemoveLeaf(proxy.m_index);
            FreeNode(proxy.m_index);
        }

        bool Update(BVHProxy proxy, const AABB& newAabb) {
            hive::Assert(proxy.IsValid(), "Invalid BVH proxy");
            BVHNode& leaf = m_nodes[proxy.m_index];
            hive::Assert(leaf.m_count > 0, "Proxy must be a leaf");

            const uint32_t itemIndex = leaf.m_left;
            m_itemAabbs[itemIndex] = newAabb;

            const AABB currentFat{leaf.m_aabbMin, leaf.m_aabbMax};
            if (detail::Contains(currentFat, newAabb))
            {
                return false;
            }

            RemoveLeaf(proxy.m_index);
            m_fatAabbs[itemIndex] = detail::FattenAABB(newAabb, detail::kFatMargin);
            leaf.m_aabbMin = m_fatAabbs[itemIndex].m_min;
            leaf.m_aabbMax = m_fatAabbs[itemIndex].m_max;

            if (m_root == detail::kInvalidNode)
            {
                m_root = proxy.m_index;
                m_parent[proxy.m_index] = detail::kInvalidNode;
            }
            else
            {
                InsertLeaf(proxy.m_index);
            }

            return true;
        }

        template <typename Callback> void QueryFrustum(const Frustum& frustum, Callback&& cb) const {
            if (m_root == detail::kInvalidNode)
            {
                return;
            }

            uint32_t stack[detail::kMaxStackDepth];
            uint32_t stackSize = 0;
            stack[stackSize++] = m_root;

            while (stackSize > 0)
            {
                const uint32_t nodeIndex = stack[--stackSize];
                const BVHNode& node = m_nodes[nodeIndex];
                const AABB box{node.m_aabbMin, node.m_aabbMax};

                if (!IsVisible(frustum, box))
                {
                    continue;
                }

                if (node.m_count > 0)
                {
                    for (uint32_t i = 0; i < node.m_count; ++i)
                    {
                        const uint32_t itemIndex = m_items[node.m_left + i];
                        if (IsVisible(frustum, m_itemAabbs[itemIndex]))
                        {
                            cb(itemIndex);
                        }
                    }
                }
                else
                {
                    hive::Assert(stackSize + 2 <= detail::kMaxStackDepth, "BVH stack overflow");
                    stack[stackSize++] = node.m_left;
                    stack[stackSize++] = m_right[nodeIndex];
                }
            }
        }

        template <typename Callback> void QueryRay(Float3 origin, Float3 direction, float maxT, Callback&& cb) const {
            if (m_root == detail::kInvalidNode)
            {
                return;
            }

            const Float3 invDir{
                Abs(direction.m_x) > kEpsilon ? 1.f / direction.m_x : (direction.m_x >= 0.f ? FLT_MAX : -FLT_MAX),
                Abs(direction.m_y) > kEpsilon ? 1.f / direction.m_y : (direction.m_y >= 0.f ? FLT_MAX : -FLT_MAX),
                Abs(direction.m_z) > kEpsilon ? 1.f / direction.m_z : (direction.m_z >= 0.f ? FLT_MAX : -FLT_MAX),
            };

            uint32_t stack[detail::kMaxStackDepth];
            uint32_t stackSize = 0;
            stack[stackSize++] = m_root;

            while (stackSize > 0)
            {
                const uint32_t nodeIndex = stack[--stackSize];
                const BVHNode& node = m_nodes[nodeIndex];
                const AABB box{node.m_aabbMin, node.m_aabbMax};

                if (!detail::RayAABB(origin, invDir, maxT, box))
                {
                    continue;
                }

                if (node.m_count > 0)
                {
                    for (uint32_t i = 0; i < node.m_count; ++i)
                    {
                        const uint32_t itemIndex = m_items[node.m_left + i];
                        if (detail::RayAABB(origin, invDir, maxT, m_itemAabbs[itemIndex]))
                        {
                            cb(itemIndex);
                        }
                    }
                }
                else
                {
                    hive::Assert(stackSize + 2 <= detail::kMaxStackDepth, "BVH stack overflow");
                    stack[stackSize++] = node.m_left;
                    stack[stackSize++] = m_right[nodeIndex];
                }
            }
        }

        template <typename Callback> void QueryAABB(const AABB& query, Callback&& cb) const {
            if (m_root == detail::kInvalidNode)
            {
                return;
            }

            uint32_t stack[detail::kMaxStackDepth];
            uint32_t stackSize = 0;
            stack[stackSize++] = m_root;

            while (stackSize > 0)
            {
                const uint32_t nodeIndex = stack[--stackSize];
                const BVHNode& node = m_nodes[nodeIndex];
                const AABB box{node.m_aabbMin, node.m_aabbMax};

                if (!detail::AABBOverlaps(query, box))
                {
                    continue;
                }

                if (node.m_count > 0)
                {
                    for (uint32_t i = 0; i < node.m_count; ++i)
                    {
                        const uint32_t itemIndex = m_items[node.m_left + i];
                        if (detail::AABBOverlaps(query, m_itemAabbs[itemIndex]))
                        {
                            cb(itemIndex);
                        }
                    }
                }
                else
                {
                    hive::Assert(stackSize + 2 <= detail::kMaxStackDepth, "BVH stack overflow");
                    stack[stackSize++] = node.m_left;
                    stack[stackSize++] = m_right[nodeIndex];
                }
            }
        }

        void Refit() {
            if (m_root == detail::kInvalidNode)
            {
                return;
            }

            RefitNode(m_root);
        }

        void Clear() {
            m_nodes.Clear();
            m_right.Clear();
            m_parent.Clear();
            m_items.Clear();
            m_itemAabbs.Clear();
            m_fatAabbs.Clear();
            m_freeList.Clear();
            m_root = detail::kInvalidNode;
            m_itemCount = 0;
        }

        [[nodiscard]] uint32_t NodeCount() const {
            return static_cast<uint32_t>(m_nodes.Size()) - static_cast<uint32_t>(m_freeList.Size());
        }

        [[nodiscard]] uint32_t ItemCount() const { return m_itemCount; }

        [[nodiscard]] bool IsEmpty() const { return m_root == detail::kInvalidNode; }

        [[nodiscard]] AABB& ItemAABB(uint32_t idx) { return m_itemAabbs[idx]; }

        [[nodiscard]] const AABB& ItemAABB(uint32_t idx) const { return m_itemAabbs[idx]; }

    private:
        [[nodiscard]] uint32_t AllocNode() {
            if (!m_freeList.IsEmpty())
            {
                const uint32_t index = m_freeList.Back();
                m_freeList.PopBack();
                m_nodes[index] = BVHNode{};
                m_right[index] = detail::kInvalidNode;
                m_parent[index] = detail::kInvalidNode;
                return index;
            }

            const uint32_t index = static_cast<uint32_t>(m_nodes.Size());
            m_nodes.PushBack(BVHNode{});
            m_right.PushBack(detail::kInvalidNode);
            m_parent.PushBack(detail::kInvalidNode);
            return index;
        }

        void FreeNode(uint32_t idx) { m_freeList.PushBack(idx); }

        void UpdateLeafBounds(uint32_t nodeIndex) {
            BVHNode& node = m_nodes[nodeIndex];
            hive::Assert(node.m_count > 0, "UpdateLeafBounds called on internal node");

            node.m_aabbMin = {FLT_MAX, FLT_MAX, FLT_MAX};
            node.m_aabbMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
            for (uint32_t i = 0; i < node.m_count; ++i)
            {
                const AABB& aabb = m_itemAabbs[m_items[node.m_left + i]];
                node.m_aabbMin.m_x = Min(node.m_aabbMin.m_x, aabb.m_min.m_x);
                node.m_aabbMin.m_y = Min(node.m_aabbMin.m_y, aabb.m_min.m_y);
                node.m_aabbMin.m_z = Min(node.m_aabbMin.m_z, aabb.m_min.m_z);
                node.m_aabbMax.m_x = Max(node.m_aabbMax.m_x, aabb.m_max.m_x);
                node.m_aabbMax.m_y = Max(node.m_aabbMax.m_y, aabb.m_max.m_y);
                node.m_aabbMax.m_z = Max(node.m_aabbMax.m_z, aabb.m_max.m_z);
            }
        }

        void Subdivide(uint32_t nodeIndex) {
            const BVHNode& node = m_nodes[nodeIndex];
            if (node.m_count <= detail::kMaxLeafItems)
            {
                return;
            }

            AABB centroidBounds{{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
            for (uint32_t i = 0; i < node.m_count; ++i)
            {
                const Float3 centroid = detail::Center(m_itemAabbs[m_items[node.m_left + i]]);
                centroidBounds.m_min.m_x = Min(centroidBounds.m_min.m_x, centroid.m_x);
                centroidBounds.m_min.m_y = Min(centroidBounds.m_min.m_y, centroid.m_y);
                centroidBounds.m_min.m_z = Min(centroidBounds.m_min.m_z, centroid.m_z);
                centroidBounds.m_max.m_x = Max(centroidBounds.m_max.m_x, centroid.m_x);
                centroidBounds.m_max.m_y = Max(centroidBounds.m_max.m_y, centroid.m_y);
                centroidBounds.m_max.m_z = Max(centroidBounds.m_max.m_z, centroid.m_z);
            }

            const float extent[3] = {
                centroidBounds.m_max.m_x - centroidBounds.m_min.m_x,
                centroidBounds.m_max.m_y - centroidBounds.m_min.m_y,
                centroidBounds.m_max.m_z - centroidBounds.m_min.m_z,
            };
            const float centroidMin[3] = {
                centroidBounds.m_min.m_x,
                centroidBounds.m_min.m_y,
                centroidBounds.m_min.m_z,
            };

            float bestCost = FLT_MAX;
            int bestAxis = -1;
            uint32_t bestSplit = 0;

            for (int axis = 0; axis < 3; ++axis)
            {
                if (extent[axis] < kEpsilon)
                {
                    continue;
                }

                detail::SAHBin bins[detail::kSAHBinCount]{};
                const float invExtent = 1.f / extent[axis];

                for (uint32_t i = 0; i < node.m_count; ++i)
                {
                    const uint32_t itemIndex = m_items[node.m_left + i];
                    const Float3 centroid = detail::Center(m_itemAabbs[itemIndex]);
                    const float position[3] = {centroid.m_x, centroid.m_y, centroid.m_z};
                    uint32_t bin =
                        static_cast<uint32_t>((position[axis] - centroidMin[axis]) * invExtent * detail::kSAHBinCount);
                    if (bin >= detail::kSAHBinCount)
                    {
                        bin = detail::kSAHBinCount - 1;
                    }

                    ++bins[bin].m_count;
                    bins[bin].m_bounds = detail::Union(bins[bin].m_bounds, m_itemAabbs[itemIndex]);
                }

                float leftArea[detail::kSAHBinCount - 1];
                uint32_t leftCount[detail::kSAHBinCount - 1];
                {
                    AABB accumulated{{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
                    uint32_t count = 0;
                    for (uint32_t split = 0; split < detail::kSAHBinCount - 1; ++split)
                    {
                        accumulated = detail::Union(accumulated, bins[split].m_bounds);
                        count += bins[split].m_count;
                        leftArea[split] = detail::SurfaceArea(accumulated);
                        leftCount[split] = count;
                    }
                }

                float rightArea[detail::kSAHBinCount - 1];
                uint32_t rightCount[detail::kSAHBinCount - 1];
                {
                    AABB accumulated{{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
                    uint32_t count = 0;
                    for (uint32_t split = detail::kSAHBinCount - 1; split > 0; --split)
                    {
                        accumulated = detail::Union(accumulated, bins[split].m_bounds);
                        count += bins[split].m_count;
                        rightArea[split - 1] = detail::SurfaceArea(accumulated);
                        rightCount[split - 1] = count;
                    }
                }

                for (uint32_t split = 0; split < detail::kSAHBinCount - 1; ++split)
                {
                    if (leftCount[split] == 0 || rightCount[split] == 0)
                    {
                        continue;
                    }

                    const float cost = leftArea[split] * static_cast<float>(leftCount[split]) +
                                       rightArea[split] * static_cast<float>(rightCount[split]);
                    if (cost < bestCost)
                    {
                        bestCost = cost;
                        bestAxis = axis;
                        bestSplit = split;
                    }
                }
            }

            if (bestAxis < 0)
            {
                return;
            }

            const float leafCost =
                detail::SurfaceArea(AABB{node.m_aabbMin, node.m_aabbMax}) * static_cast<float>(node.m_count);
            if (bestCost >= leafCost)
            {
                return;
            }

            const float splitPosition =
                centroidMin[bestAxis] +
                (static_cast<float>(bestSplit + 1) / static_cast<float>(detail::kSAHBinCount)) * extent[bestAxis];

            const uint32_t first = node.m_left;
            const uint32_t last = first + node.m_count;
            uint32_t middle = first;
            for (uint32_t i = first; i < last; ++i)
            {
                const Float3 centroid = detail::Center(m_itemAabbs[m_items[i]]);
                const float position[3] = {centroid.m_x, centroid.m_y, centroid.m_z};
                if (position[bestAxis] < splitPosition)
                {
                    const uint32_t tmp = m_items[i];
                    m_items[i] = m_items[middle];
                    m_items[middle] = tmp;
                    ++middle;
                }
            }

            if (middle == first || middle == last)
            {
                middle = first + node.m_count / 2;
            }

            const uint32_t leftCount = middle - first;
            const uint32_t rightCount = node.m_count - leftCount;

            const uint32_t leftChild = AllocNode();
            const uint32_t rightChild = AllocNode();

            m_nodes[leftChild].m_left = first;
            m_nodes[leftChild].m_count = leftCount;
            m_parent[leftChild] = nodeIndex;

            m_nodes[rightChild].m_left = middle;
            m_nodes[rightChild].m_count = rightCount;
            m_parent[rightChild] = nodeIndex;

            m_nodes[nodeIndex].m_left = leftChild;
            m_nodes[nodeIndex].m_count = 0;
            m_right[nodeIndex] = rightChild;

            UpdateLeafBounds(leftChild);
            UpdateLeafBounds(rightChild);
            Subdivide(leftChild);
            Subdivide(rightChild);
        }

        void InsertLeaf(uint32_t leafIndex) {
            const AABB leafBox{m_nodes[leafIndex].m_aabbMin, m_nodes[leafIndex].m_aabbMax};

            uint32_t bestSibling = m_root;
            float bestCost =
                detail::SurfaceArea(detail::Union(AABB{m_nodes[m_root].m_aabbMin, m_nodes[m_root].m_aabbMax}, leafBox));

            struct Entry
            {
                uint32_t m_node;
                float m_inherited;
            };

            Entry stack[detail::kMaxStackDepth];
            uint32_t stackSize = 0;
            stack[stackSize++] = {m_root, 0.f};

            while (stackSize > 0)
            {
                const Entry entry = stack[--stackSize];
                const uint32_t nodeIndex = entry.m_node;
                const float inheritedCost = entry.m_inherited;
                const BVHNode& node = m_nodes[nodeIndex];
                const AABB nodeBox{node.m_aabbMin, node.m_aabbMax};
                const AABB combined = detail::Union(nodeBox, leafBox);
                const float directCost = detail::SurfaceArea(combined);
                const float totalCost = directCost + inheritedCost;

                if (totalCost < bestCost)
                {
                    bestCost = totalCost;
                    bestSibling = nodeIndex;
                }

                const float delta = directCost - detail::SurfaceArea(nodeBox);
                const float lowerBound = detail::SurfaceArea(leafBox) + inheritedCost + delta;
                if (lowerBound >= bestCost)
                {
                    continue;
                }

                if (node.m_count == 0)
                {
                    const float childInherited = inheritedCost + delta;
                    hive::Assert(stackSize + 2 <= detail::kMaxStackDepth, "BVH insert stack overflow");
                    stack[stackSize++] = {node.m_left, childInherited};
                    stack[stackSize++] = {m_right[nodeIndex], childInherited};
                }
            }

            const uint32_t oldParent = m_parent[bestSibling];
            const uint32_t newParent = AllocNode();

            const AABB siblingBox{m_nodes[bestSibling].m_aabbMin, m_nodes[bestSibling].m_aabbMax};
            const AABB combined = detail::Union(siblingBox, leafBox);
            m_nodes[newParent].m_aabbMin = combined.m_min;
            m_nodes[newParent].m_aabbMax = combined.m_max;
            m_nodes[newParent].m_left = bestSibling;
            m_nodes[newParent].m_count = 0;
            m_right[newParent] = leafIndex;
            m_parent[newParent] = oldParent;

            if (oldParent != detail::kInvalidNode)
            {
                if (m_nodes[oldParent].m_left == bestSibling)
                {
                    m_nodes[oldParent].m_left = newParent;
                }
                else
                {
                    m_right[oldParent] = newParent;
                }
            }
            else
            {
                m_root = newParent;
            }

            m_parent[bestSibling] = newParent;
            m_parent[leafIndex] = newParent;

            RefitAncestors(newParent);
        }

        void RemoveLeaf(uint32_t leafIndex) {
            if (leafIndex == m_root)
            {
                m_root = detail::kInvalidNode;
                m_parent[leafIndex] = detail::kInvalidNode;
                return;
            }

            const uint32_t parentIndex = m_parent[leafIndex];
            hive::Assert(parentIndex != detail::kInvalidNode, "Leaf has no parent");

            const uint32_t siblingIndex =
                m_nodes[parentIndex].m_left == leafIndex ? m_right[parentIndex] : m_nodes[parentIndex].m_left;
            const uint32_t grandParentIndex = m_parent[parentIndex];

            if (grandParentIndex != detail::kInvalidNode)
            {
                if (m_nodes[grandParentIndex].m_left == parentIndex)
                {
                    m_nodes[grandParentIndex].m_left = siblingIndex;
                }
                else
                {
                    m_right[grandParentIndex] = siblingIndex;
                }

                m_parent[siblingIndex] = grandParentIndex;
                m_parent[leafIndex] = detail::kInvalidNode;
                FreeNode(parentIndex);
                RefitAncestors(grandParentIndex);
            }
            else
            {
                m_root = siblingIndex;
                m_parent[siblingIndex] = detail::kInvalidNode;
                m_parent[leafIndex] = detail::kInvalidNode;
                FreeNode(parentIndex);
            }
        }

        void RefitAncestors(uint32_t nodeIndex) {
            uint32_t index = nodeIndex;
            while (index != detail::kInvalidNode)
            {
                BVHNode& node = m_nodes[index];
                if (node.m_count == 0)
                {
                    const BVHNode& left = m_nodes[node.m_left];
                    const BVHNode& right = m_nodes[m_right[index]];
                    node.m_aabbMin = {Min(left.m_aabbMin.m_x, right.m_aabbMin.m_x),
                                      Min(left.m_aabbMin.m_y, right.m_aabbMin.m_y),
                                      Min(left.m_aabbMin.m_z, right.m_aabbMin.m_z)};
                    node.m_aabbMax = {Max(left.m_aabbMax.m_x, right.m_aabbMax.m_x),
                                      Max(left.m_aabbMax.m_y, right.m_aabbMax.m_y),
                                      Max(left.m_aabbMax.m_z, right.m_aabbMax.m_z)};
                }

                index = m_parent[index];
            }
        }

        [[nodiscard]] AABB RefitNode(uint32_t nodeIndex) {
            BVHNode& node = m_nodes[nodeIndex];
            if (node.m_count > 0)
            {
                node.m_aabbMin = {FLT_MAX, FLT_MAX, FLT_MAX};
                node.m_aabbMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
                for (uint32_t i = 0; i < node.m_count; ++i)
                {
                    const AABB& aabb = m_itemAabbs[m_items[node.m_left + i]];
                    node.m_aabbMin.m_x = Min(node.m_aabbMin.m_x, aabb.m_min.m_x);
                    node.m_aabbMin.m_y = Min(node.m_aabbMin.m_y, aabb.m_min.m_y);
                    node.m_aabbMin.m_z = Min(node.m_aabbMin.m_z, aabb.m_min.m_z);
                    node.m_aabbMax.m_x = Max(node.m_aabbMax.m_x, aabb.m_max.m_x);
                    node.m_aabbMax.m_y = Max(node.m_aabbMax.m_y, aabb.m_max.m_y);
                    node.m_aabbMax.m_z = Max(node.m_aabbMax.m_z, aabb.m_max.m_z);
                }

                return {node.m_aabbMin, node.m_aabbMax};
            }

            const AABB leftBounds = RefitNode(node.m_left);
            const AABB rightBounds = RefitNode(m_right[nodeIndex]);
            const AABB combined = detail::Union(leftBounds, rightBounds);
            node.m_aabbMin = combined.m_min;
            node.m_aabbMax = combined.m_max;
            return combined;
        }

        wax::Vector<BVHNode> m_nodes;
        wax::Vector<uint32_t> m_right;
        wax::Vector<uint32_t> m_parent;
        wax::Vector<uint32_t> m_items;
        wax::Vector<AABB> m_itemAabbs;
        wax::Vector<AABB> m_fatAabbs;
        wax::Vector<uint32_t> m_freeList;

        uint32_t m_root{detail::kInvalidNode};
        uint32_t m_itemCount{0};
    };

    using BuddyBVH = BVH<comb::BuddyAllocator>;
} // namespace hive::math
