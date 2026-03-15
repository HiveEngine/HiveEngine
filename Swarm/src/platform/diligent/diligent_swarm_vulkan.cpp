#include <swarm/platform/diligent_swarm.h>
#include <swarm/precomp.h>

#include <EngineFactoryVk.h>
namespace swarm
{
    bool InitRenderContextCommon(RenderContext* renderContext)
    {
        auto* factory = Diligent::GetEngineFactoryVk();

        Diligent::EngineVkCreateInfo createInfo{};
        factory->CreateDeviceAndContextsVk(createInfo, &renderContext->m_device, &renderContext->m_context);

        return true;
    }
} // namespace swarm
