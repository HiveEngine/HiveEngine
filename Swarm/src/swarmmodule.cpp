#include <swarm/precomp.h>
#include <swarm/swarmmodule.h>

namespace swarm
{
    SwarmModule::SwarmModule() : m_moduleAllocator("Swarm", 1 * 1024 * 1024)
    {
    }
}
