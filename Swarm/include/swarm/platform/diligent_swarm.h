#pragma once
#include <RenderDevice.h>
namespace swarm
{
    struct RenderContext
    {
        Diligent::IRenderDevice* m_device{nullptr};
        Diligent::IDeviceContext* m_context{nullptr};

        Diligent::ISwapChain* m_swapchain{nullptr}; // Can be null if offscreen rendering

        // TEMP
        Diligent::IPipelineState* m_pipeline{nullptr};
    };
} // namespace swarm
