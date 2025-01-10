#include <rendering/RendererPlatform.h>


#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <array>
#include <core/Logger.h>
#include "vulkan_renderpass.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"

#include <vulkan/vulkan.h>

namespace hive::vk
{
    bool create_renderpass(const VulkanDevice& device, const VulkanSwapchain& swapchain, VkRenderPass &out_renderpass)
    {

        //Color texture
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

        //Depth texture
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = find_depth_format(device);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcAccessMask = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachment_descriptions = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<u32>(attachment_descriptions.size());
        renderPassInfo.pAttachments = attachment_descriptions.data();
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
            std::array<VkImageView, 2> attachments = {
                swapchain.image_views[i],
                swapchain.depth_image.vk_image_view
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = render_pass;
            framebufferInfo.attachmentCount = static_cast<u32>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
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
