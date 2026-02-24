#pragma once
#include <RenderDevice.h>
namespace swarm
{
    struct RenderContext
    {
        Diligent::IRenderDevice *device_{nullptr};
        Diligent::IDeviceContext *context_{nullptr};

        Diligent::ISwapChain *swapchain_{nullptr}; //Can be null if offscreen rendering

        //TEMP
        Diligent::IPipelineState *pipeline_{nullptr};
    };
}
