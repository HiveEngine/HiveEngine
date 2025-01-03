#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_framebuffer.h"
#include "vulkan_swapchain.h"
#include "vulkan_device.h"
#include "vulkan_renderpass.h"
#include <core/Logger.h>

void hive::vk::create_framebuffer(const Device& device, const Swapchain& swapchain, const RenderPass& render_pass,  Framebuffer& framebuffer)
{
    framebuffer.vk_framebuffers.resize(swapchain.image_views.size());

    for (size_t i = 0; i < swapchain.image_views.size(); i++)
    {
        VkImageView attachments[] = {
            swapchain.image_views[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = render_pass.vk_render_pass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain.extent_2d.width;
        framebufferInfo.height = swapchain.extent_2d.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device.logical_device, &framebufferInfo, nullptr, &framebuffer.vk_framebuffers[i]) !=
            VK_SUCCESS)
        {
            LOG_ERROR("failed to create framebuffer!");
            break;
        }
    }
}

#endif
