#pragma once

#include <wax/containers/vector.h>
#include <comb/linear_allocator.h>
#include <comb/pool_allocator.h>
#include <comb/stack_allocator.h>
#include <comb/slab_allocator.h>
#include <comb/buddy_allocator.h>

/**
 * Type aliases for common Vector + Allocator combinations
 *
 * These aliases provide convenient shorthands for frequently used
 * Vector configurations while still requiring explicit allocator instances.
 *
 * Usage:
 * @code
 *   comb::LinearAllocator alloc{1024 * 1024};
 *
 *   // Using alias (cleaner)
 *   wax::LinearVector<int> vec{alloc};
 *   vec.PushBack(42);
 *
 *   // Equivalent to:
 *   wax::Vector<int, comb::LinearAllocator> vec2{alloc};
 * @endcode
 *
 * Note: You still need to provide an allocator instance at construction.
 * This is intentional - it ensures explicit control over memory allocation.
 */

namespace wax
{
    // Linear allocator (best for frame-temp data)
    template<typename T>
    using LinearVector = Vector<T, comb::LinearAllocator>;

    // Stack allocator (best for LIFO allocation patterns)
    template<typename T>
    using StackVector = Vector<T, comb::StackAllocator>;

    // Buddy allocator (best for general-purpose with low fragmentation)
    template<typename T>
    using BuddyVector = Vector<T, comb::BuddyAllocator>;
}
