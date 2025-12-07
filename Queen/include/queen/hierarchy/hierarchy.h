#pragma once

#include <queen/hierarchy/parent.h>
#include <queen/hierarchy/children.h>
#include <comb/buddy_allocator.h>

namespace queen
{
    // Forward declaration
    class World;

    // Type alias for Children using World's persistent allocator
    using Children = ChildrenT<comb::BuddyAllocator>;
}
