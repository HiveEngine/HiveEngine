#include <comb/default_allocator.h>

#include <nectar/nectar_module.h>

namespace nectar
{
    comb::DefaultAllocator& GetAllocator()
    {
        static comb::ModuleAllocator s_alloc{"Nectar", 64 * 1024 * 1024};
        return s_alloc.Get();
    }
} // namespace nectar
