#include <swarm/precomp.h>
#include <swarm/device.h>

#include <swarm/instance.h>
#include <swarm/surface.h>
namespace swarm
{
    Device::Device(Instance const& instance, Surface const& surface, DeviceDescription description)
    {
        vkb::PhysicalDeviceSelector deviceSelector{instance, surface};

        const auto deviceSelectorRes = deviceSelector. select();

        if (!deviceSelectorRes.has_value())
            return;

        m_PhysicalDevice = deviceSelectorRes.value();

        vkb::DeviceBuilder deviceBuilder{m_PhysicalDevice};

        const auto deviceBuilderRes = deviceBuilder.build();

        if (!deviceBuilderRes.has_value())
            return;

        m_Device = deviceBuilderRes.value();
    }

    Device::~Device()
    {
        vkb::destroy_device(m_Device);
    }

    bool Device::IsValid() const
    {
        return m_Device.device != VK_NULL_HANDLE;
    }
}
