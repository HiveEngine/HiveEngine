#include <waggle/precomp.h>

#include <hive/core/moduleregistry.h>

#include <terra/terramodule.h>

namespace waggle
{
    void RegisterModule()
    {
        terra::RegisterTerraModule();
    }
} // namespace waggle
