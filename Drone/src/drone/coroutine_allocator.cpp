#include <drone/coroutine_allocator.h>

namespace drone
{
    CoroutineAllocator& GetCoroutineAllocator()
    {
        static CoroutineSlabAllocator s_slab;
        static CoroutineAllocator s_allocator{s_slab};
        return s_allocator;
    }
} // namespace drone
