#include <swarm/precomp.h>
#include <swarm/platform/diligent_swarm.h>

#include <EngineFactoryVk.h>
namespace swarm
{
    bool InitRenderContextCommon(RenderContext *renderContext)
    {
        auto* factory = Diligent::GetEngineFactoryVk();

        Diligent::EngineVkCreateInfo createInfo{};
        factory->CreateDeviceAndContextsVk(createInfo, &renderContext->device_, &renderContext->context_);


        return true;
    }
}