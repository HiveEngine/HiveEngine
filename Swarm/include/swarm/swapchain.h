#pragma once

namespace swarm
{
    class Surface;
    class Device;
    struct SwapchainDescription
    {
        Surface& surface;
    };

    class Swapchain
    {
    public:
        Swapchain(Device& device, SwapchainDescription description);
        ~Swapchain();

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;

        Swapchain(Swapchain&&) = default;
        Swapchain& operator=(Swapchain&&) = default;

    protected:
        Device& m_Device;
    private:
        #include <swarm/swapchainbackend.inl>
    };
}