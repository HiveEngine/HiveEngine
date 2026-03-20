#pragma once

#include <drone/work_stealing_deque.h>

namespace queen
{
    template <typename T, comb::Allocator Allocator> using CircularBuffer = drone::CircularBuffer<T, Allocator>;

    template <typename T, comb::Allocator Allocator> using WorkStealingDeque = drone::WorkStealingDeque<T, Allocator>;
} // namespace queen
