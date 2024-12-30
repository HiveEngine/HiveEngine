#pragma once
#include "vulkan_types.h"
#include <vulkan/vulkan.h>

#include <vector>
namespace hive::vk
{

    void pick_physical_device(const PhysicalDeviceRequirements& requirements, VulkanDevice& vulkan_device, const VkSurfaceKHR &surface, const VkInstance& instance);

    bool is_device_suitable(const VkPhysicalDevice &device, const VkSurfaceKHR &surface_khr, const PhysicalDeviceRequirements &requirements,
                          PhysicalDeviceFamilyQueueInfo &out_familyQueueInfo);

    void create_logical_device(const VkInstance& instance, VulkanDevice& device);
}