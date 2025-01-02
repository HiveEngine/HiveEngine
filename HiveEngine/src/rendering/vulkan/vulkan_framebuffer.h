#pragma once
#include <hvpch.h>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace hive::vk
{
    struct VulkanDevice;
    struct VulkanSwapchain;
}

namespace hive::vk
{
    bool create_framebuffer(std::vector<VkFramebuffer> &framebuffers, const VulkanDevice &device,
                            const VulkanSwapchain &swapchain, const VkRenderPass &render_pass);

    void destroy_framebuffer(const VulkanDevice& device, const VkFramebuffer &framebuffer);
}
