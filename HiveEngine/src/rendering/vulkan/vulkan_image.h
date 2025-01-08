#pragma once

#include <vulkan/vulkan.h>

namespace hive::vk
{
    struct VulkanDevice;
    struct VulkanImage;
    struct VulkanBuffer;

    bool create_image(const VulkanDevice &device,
                      u32 tex_width,
                      u32 tex_height,
                      VkFormat format,
                      VkImageTiling tiling,
                      VkImageUsageFlags usage,
                      VkMemoryPropertyFlags memory_property,
                      VulkanImage &image);

    bool create_image_view(const VulkanDevice &device, const VkImage &image, const VkFormat &format,
                           VkImageAspectFlags aspect_flags, VkImageView &image_view);
    bool create_image_sampler(const VulkanDevice& device, VkSampler &sampler);

    void transition_image_layout(const VulkanDevice &device,
                                 VkFormat format,
                                 VkImageLayout old_layout,
                                 VkImageLayout new_layout,
                                 VulkanImage &image);

    void copy_buffer_to_image(const VulkanDevice& device, VulkanBuffer &buffer, VulkanImage &image, u32 width, u32 height);

    void destroy_image(const VulkanDevice& device, VulkanImage &image);
}
