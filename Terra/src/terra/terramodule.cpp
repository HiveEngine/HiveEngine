#include <terra/precomp.h>
#include <terra/terramodule.h>

namespace terra
{
    TerraModule::TerraModule() : m_moduleAllocator("Terra", 1 * 1024 * 1024) //1 MBB
    {
    }
} // namespace terra