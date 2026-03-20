#pragma once

#include <drone/mpmc_queue.h>

namespace queen
{
    template <typename T, comb::Allocator Allocator> using MPMCQueue = drone::MPMCQueue<T, Allocator>;
} // namespace queen
