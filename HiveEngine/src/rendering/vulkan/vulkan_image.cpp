#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_image.h"
#include <vulkan/vulkan.h>

namespace hive::vk
{
    void create_image_view(const VulkanDevice& vulkan_device, VulkanSwapchain &vulkan_swapchain)
    {
        vulkan_swapchain.image_view_count = vulkan_swapchain.image_count;
        for(u32 i = 0; i < vulkan_swapchain.image_view_count; i++)
        {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = vulkan_swapchain.images[i];
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = vulkan_swapchain.format;

            //Default color mapping. We can do stuff like monochrome texture...
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            //Describe how the image should be accesssed
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;

            VkResult result = vkCreateImageView(vulkan_device.device_, &create_info, nullptr, &vulkan_swapchain.image_view[i]);
            if(result != VK_SUCCESS)
            {
                vulkan_swapchain.image_view_count = -1;
                break;
            }
        }
    }
}
#endif
