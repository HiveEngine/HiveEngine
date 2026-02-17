#include <nectar/nectar_module.h>
#include <comb/default_allocator.h>

namespace nectar
{
    comb::DefaultAllocator& GetAllocator()
    {
        static comb::ModuleAllocator alloc{"Nectar", 64 * 1024 * 1024};
        return alloc.Get();
    }
}
