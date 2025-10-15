#pragma once

namespace swarm
{
    struct DeviceDescription
    {

    };

    class Instance;
    class Surface;
    class Device
    {
    public:
        Device(Instance const& instance, Surface const& surface, DeviceDescription description);
        ~Device();

        bool IsValid() const;

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;

    private:
        #include <swarm/devicebackend.inl>

    };
}
