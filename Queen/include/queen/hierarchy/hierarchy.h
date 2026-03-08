#pragma once

#include <comb/buddy_allocator.h>

#include <queen/hierarchy/children.h>
#include <queen/hierarchy/parent.h>

namespace queen
{
    // Forward declaration
    class World;

    // Type alias for Children using World's persistent allocator
    using Children = ChildrenT<comb::BuddyAllocator>;
} // namespace queen
