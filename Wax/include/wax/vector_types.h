#pragma once

#include <comb/buddy_allocator.h>
#include <comb/default_allocator.h>
#include <comb/linear_allocator.h>
#include <comb/pool_allocator.h>
#include <comb/slab_allocator.h>
#include <comb/stack_allocator.h>

#include <wax/containers/vector.h>

// Type aliases — all resolve to Vector<T> since the allocator is runtime.
// Kept for readability at call sites (signals intended allocator type).

namespace wax
{
    // Linear allocator (best for frame-temp data)
    template <typename T> using LinearVector = Vector<T>;

    // Stack allocator (best for LIFO allocation patterns)
    template <typename T> using StackVector = Vector<T>;

    // Buddy allocator (best for general-purpose with low fragmentation)
    template <typename T> using BuddyVector = Vector<T>;
} // namespace wax
