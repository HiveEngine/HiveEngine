#pragma once

namespace hive::vk
{
    struct Device;
    struct Swapchain;

    struct RenderPass
    {
        VkRenderPass vk_render_pass;

    };

    void create_render_pass(const Device &device, const Swapchain &swapchain, RenderPass &render_pass);
}