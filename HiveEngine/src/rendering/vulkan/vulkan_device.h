#pragma once

#include <vulkan/vulkan.h>

namespace hive::vk
{
    struct VulkanDevice;
}

namespace hive::vk
{
    bool create_device(const VkInstance& instance, const VkSurfaceKHR& surface_khr, VulkanDevice& device);
    void destroy_device(VulkanDevice &device);
}
