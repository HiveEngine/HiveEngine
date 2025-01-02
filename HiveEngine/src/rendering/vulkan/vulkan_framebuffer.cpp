#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <core/Logger.h>
#include "vulkan_types.h"
#include "vulkan_framebuffer.h"

bool hive::vk::create_framebuffer(std::vector<VkFramebuffer> &framebuffers, const VulkanDevice &device, const VulkanSwapchain &swapchain, const VkRenderPass& render_pass)
{
    framebuffers.resize(swapchain.image_view_count);

    for(i32 i = 0; i < swapchain.image_view_count; i++)
    {
        VkImageView attachments[] = {
            swapchain.image_view[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = render_pass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain.extent.width;
        framebufferInfo.height = swapchain.extent.height;

        if (vkCreateFramebuffer(device.device_, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
        {
            LOG_ERROR("Could not create the framebuffer")
            return false;
        }
    }

    return true;
}

void hive::vk::destroy_framebuffer(const VulkanDevice &device, const VkFramebuffer &framebuffer)
{
    vkDestroyFramebuffer(device.device_, framebuffer, nullptr);
}

#endif


