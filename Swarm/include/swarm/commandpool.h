#pragma once

namespace swarm
{
    class Device;
    class Surface;

    class CommandPool
    {
    public:
        CommandPool(Device& device, Surface& surface);
        ~CommandPool();

    protected:
        Device& m_Device;

    private:
        #include <swarm/commandpoolbackend.inl>

    };
}