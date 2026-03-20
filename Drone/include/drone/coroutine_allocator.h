#pragma once

#include <comb/slab_allocator.h>
#include <comb/thread_safe_allocator.h>

#include <cstddef>

namespace drone
{
    // Coroutine frames vary 64–512 bytes. Slab gives O(1) alloc with no fragmentation.
    // Thread-safe via mutex (~50ns overhead). 4096 objects per size class.
    using CoroutineSlabAllocator = comb::SlabAllocator<4096, 64, 128, 256, 512, 1024>;
    using CoroutineAllocator = comb::ThreadSafeAllocator<CoroutineSlabAllocator>;

    CoroutineAllocator& GetCoroutineAllocator();

} // namespace drone
