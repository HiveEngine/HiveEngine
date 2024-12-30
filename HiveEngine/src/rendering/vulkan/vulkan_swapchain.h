#pragma once

#include <vulkan/vulkan.h>

#include "vulkan_types.h"

namespace hive
{
    class Window;
}

namespace hive::vk
{
    void create_swapchain(const VkInstance& instance, const VkSurfaceKHR &surface,  const VulkanDevice& vulkan_device, VulkanSwapchain& swapchain, const Window& window);
}
