#include <forge/forge_module.h>

namespace forge
{
    comb::DefaultAllocator& GetAllocator()
    {
        static comb::ModuleAllocator s_alloc{"Forge", 32 * 1024 * 1024};
        return s_alloc.Get();
    }
} // namespace forge
