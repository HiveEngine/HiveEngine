#pragma once
#include <vulkan/vulkan.h>

namespace hive::vk
{
    struct VulkanDevice;
    struct VulkanSwapchain;

    bool create_renderpass(const VulkanDevice &device, const VulkanSwapchain &swapchain, VkRenderPass&renderpass);
    void destroy_renderpass(const VulkanDevice& device, VkRenderPass& renderpass);
}
