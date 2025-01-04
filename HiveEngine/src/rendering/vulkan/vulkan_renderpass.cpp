#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <core/Logger.h>
#include "vulkan_renderpass.h"
#include "vulkan_types.h"

#include <vulkan/vulkan.h>

namespace hive::vk
{
    bool create_renderpass(const VulkanDevice& device, const VulkanSwapchain& swapchain, VkRenderPass &out_renderpass)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchain.image_format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device.logical_device, &renderPassInfo, nullptr, &out_renderpass) !=
            VK_SUCCESS)
        {
            LOG_ERROR("failed to create render pass!");
            return false;
        }
        return true;
    }

    bool create_framebuffer(const VulkanDevice& device, const VulkanSwapchain& swapchain, const VkRenderPass& render_pass, VulkanFramebuffer& framebuffer)
    {
        framebuffer.framebuffers.resize(swapchain.image_views.size());

        for (size_t i = 0; i < swapchain.image_views.size(); i++)
        {
            VkImageView attachments[] = {
                swapchain.image_views[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = render_pass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapchain.extent_2d.width;
            framebufferInfo.height = swapchain.extent_2d.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device.logical_device, &framebufferInfo, nullptr,
                                    &framebuffer.framebuffers[i]) !=
                VK_SUCCESS)
            {
                LOG_ERROR("failed to create framebuffer!");
                return false;
            }
        }

        return true;
    }

    void destroy_renderpass(const VulkanDevice &device, const VkRenderPass&render_pass)
    {
        vkDestroyRenderPass(device.logical_device, render_pass, nullptr);
    }

    void destroy_framebuffer(const VulkanDevice &device, VulkanFramebuffer framebuffer)
    {
        for(i32 i = 0; i < framebuffer.framebuffers.size(); i++)
        {
            vkDestroyFramebuffer(device.logical_device, framebuffer.framebuffers[i], nullptr);
        }
    }
}
#endif
