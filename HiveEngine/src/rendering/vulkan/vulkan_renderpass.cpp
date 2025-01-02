#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_renderpass.h"
#include "vulkan_types.h"
#include <core/Logger.h>

namespace hive::vk
{

    bool create_renderpass(const VulkanDevice &device, const VulkanSwapchain &swapchain, VkRenderPass &renderpass)
    {
        VkAttachmentDescription color_attachment{};
        color_attachment.format = swapchain.format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //After each frame clear the framebuffer
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        //Rendered contents is stored in memory and can be read lter

        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //We don't care about the image initial layout since it's gonna be cleared anyway
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; //Image mode to be presented to the swap chain

        VkAttachmentReference color_attachment_reference{};
        color_attachment_reference.attachment = 0; //Directly reference the layout location 0 in our fagment shader
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass_description{};
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_description.colorAttachmentCount = 1;
        subpass_description.pColorAttachments = &color_attachment_reference;

        VkRenderPassCreateInfo render_pass_create_info{};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = 1;
        render_pass_create_info.pAttachments = &color_attachment;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass_description;

        if (vkCreateRenderPass(device.device_, &render_pass_create_info, nullptr, &renderpass) != VK_SUCCESS)
        {
            LOG_ERROR("Could not create a renderpass");
            return false;
        }

        return true;
    }

    void destroy_renderpass(const VulkanDevice &device, VkRenderPass&renderpass)
    {

    }
}

#endif