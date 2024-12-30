#pragma once

#include <vulkan/vulkan.h>
namespace hive::vk
{
    struct PhysicalDeviceRequirements
    {

    };

    void pick_physical_device(const PhysicalDeviceRequirements& requirements, VkPhysicalDevice& physical_device);
}