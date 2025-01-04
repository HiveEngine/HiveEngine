#pragma once

#include <vulkan/vulkan.h>
namespace hive::vk
{

    struct VulkanDevice;
    struct VulkanSwapchain;
    struct VulkanFramebuffer;

    bool create_renderpass(const VulkanDevice& device, const VulkanSwapchain& swapchain, VkRenderPass &out_renderpass);

    bool create_framebuffer(const VulkanDevice& device, const VulkanSwapchain& swapchain, const VkRenderPass& render_pass, VulkanFramebuffer& framebuffer);
}