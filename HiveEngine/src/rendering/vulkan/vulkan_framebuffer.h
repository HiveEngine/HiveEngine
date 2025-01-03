#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

namespace hive::vk
{
    struct Swapchain;
    struct RenderPass;
    struct Device;

    struct Framebuffer
    {
        std::vector<VkFramebuffer> vk_framebuffers;
    };

    void create_framebuffer(const Device& device, const Swapchain& swapchain, const RenderPass& render_pass,  Framebuffer& framebuffer);
}
