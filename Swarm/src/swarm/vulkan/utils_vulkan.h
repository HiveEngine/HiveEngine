#pragma once

namespace swarm::vk
{
    inline VkFormat FindSupportedFormat(VkPhysicalDevice physDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }

    inline VkFormat FindDepthFormat(VkPhysicalDevice physDevice)
    {
        return FindSupportedFormat(physDevice,
             {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
             VK_IMAGE_TILING_OPTIMAL,
             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
         );
    }

    inline unsigned int FindMemoryType(VkPhysicalDevice physicalDevice, unsigned int typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (unsigned int i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        return -1;
    }


}
