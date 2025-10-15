#pragma once

namespace swarm
{
    class Device;
    class Swapchain;

    struct RenderPassDescription
    {
        Swapchain &swapchain;
    };

    //For now the renderpass expect to have the following:
    // 1 color attachment, 1 depth attachment
    class RenderPass
    {
    public:
        RenderPass(Device& device, RenderPassDescription description);
        ~RenderPass();

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;

    protected:
        Device& m_Device;
    private:
        #include <swarm/renderpassbackend.inl>
    };
}