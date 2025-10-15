#include <swarm/precomp.h>
#include <swarm/instance.h>

#include <VkBootstrap.h>

namespace swarm
{
    Instance::Instance(InstanceDescription instanceDescription)
    {
        vkb::InstanceBuilder builder{};

        if (instanceDescription.applicationName)
            builder.set_app_name(instanceDescription.applicationName);

        if (instanceDescription.applicationVersion)
            builder.set_app_version(instanceDescription.applicationVersion);

        if (instanceDescription.isDebug)
            builder.request_validation_layers();

        const auto builderRes = builder.build();
        if (!builderRes.has_value())
            return;

        m_Instance = builderRes.value();
    }

    Instance::~Instance()
    {
        vkb::destroy_instance(m_Instance);
    }

    bool Instance::IsValid() const
    {
        return m_Instance.instance != VK_NULL_HANDLE;
    }
}
