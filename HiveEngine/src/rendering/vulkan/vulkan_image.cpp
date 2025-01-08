#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <core/Logger.h>

#include "vulkan_image.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"

namespace hive::vk
{
    bool create_image(const VulkanDevice &device,
                      u32 tex_width,
                      u32 tex_height,
                      VkFormat format,
                      VkImageTiling tiling,
                      VkImageUsageFlags usage,
                      VkMemoryPropertyFlags memory_property,
                      VulkanImage &image)
    {
        VkImageCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        create_info.imageType = VK_IMAGE_TYPE_2D;
        create_info.extent.width = tex_width;
        create_info.extent.height = tex_height;
        create_info.extent.depth = 1;
        create_info.mipLevels = 1;
        create_info.arrayLayers = 1;

        create_info.format = format;
        create_info.tiling = tiling;
        create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        create_info.usage = usage;
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        create_info.flags = 0;

        if(vkCreateImage(device.logical_device, &create_info, nullptr, &image.vk_image) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create image");
            return false;
        }

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(device.logical_device, image.vk_image, &memory_requirements);

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        u32 memory_index;
        if (!findMemoryType(device, memory_requirements.memoryTypeBits,
                            memory_property,
                            memory_index))
        {
            LOG_ERROR("Vulkan: failed to find visible memory type");
            return false;
        }

        allocate_info.memoryTypeIndex = memory_index;

        if(vkAllocateMemory(device.logical_device, &allocate_info, nullptr, &image.vk_image_memory) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to allocate image memory");
            return false;
        }

        vkBindImageMemory(device.logical_device, image.vk_image, image.vk_image_memory, 0);

        return true;
    }

    bool create_image_view(const VulkanDevice &device, const VkImage &image, VkImageView &image_view)
    {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = image;
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        if(vkCreateImageView(device.logical_device, &create_info, nullptr, &image_view) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create texture image view");
            return false;
        }
        return true;
    }

    bool create_image_sampler(const VulkanDevice& device, VkSampler &sampler)
    {
        VkSamplerCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        create_info.magFilter = VK_FILTER_LINEAR;
        create_info.minFilter = VK_FILTER_LINEAR;
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.anisotropyEnable = VK_TRUE;

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device.physical_device, &properties);
        create_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        create_info.unnormalizedCoordinates = VK_FALSE;
        create_info.compareEnable = VK_FALSE;
        create_info.compareOp = VK_COMPARE_OP_ALWAYS;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info.mipLodBias = 0.0f;
        create_info.minLod = 0.0f;
        create_info.maxLod = 0.0f;

        if(vkCreateSampler(device.logical_device, &create_info, nullptr, &sampler) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create image sampler");
            return false;
        }
        return true;
    }

    void transition_image_layout(const VulkanDevice &device, VkFormat format, VkImageLayout old_layout,
        VkImageLayout new_layout, VulkanImage& image)
    {
        VkCommandBuffer command_buffer = begin_single_command(device);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image.vk_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0; // TODO
        barrier.dstAccessMask = 0; // TODO

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout ==
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else
        {
            LOG_ERROR("Vulkan: unsupported layout transition!");
            return;
        }

        vkCmdPipelineBarrier(
            command_buffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );


        end_single_command(device, command_buffer);
    }

    void copy_buffer_to_image(const VulkanDevice& device, VulkanBuffer &buffer, VulkanImage &image, u32 width, u32 height)
    {
        VkCommandBuffer command_buffer = begin_single_command(device);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(
            command_buffer,
            buffer.vk_buffer,
            image.vk_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        end_single_command(device, command_buffer);
    }

    void destroy_image(const VulkanDevice &device, VulkanImage &image)
    {
        vkDestroySampler(device.logical_device, image.vk_sampler, nullptr);
        vkDestroyImageView(device.logical_device, image.vk_image_view, nullptr);
        vkDestroyImage(device.logical_device, image.vk_image, nullptr);
        vkFreeMemory(device.logical_device, image.vk_image_memory, nullptr);

        image.vk_image = VK_NULL_HANDLE;
        image.vk_image_memory = VK_NULL_HANDLE;
    }
}
#endif
