#include <waggle/precomp.h>

#include <hive/core/moduleregistry.h>

#include <terra/terramodule.h>
#include <swarm/swarmmodule.h>

namespace waggle
{
    void RegisterModule()
    {
        terra::RegisterTerraModule();
        swarm::RegisterSwarmModule();
    }
} // namespace waggle
