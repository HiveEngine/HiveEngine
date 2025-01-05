#pragma once
#include <hvpch.h>
namespace hive::vk
{
    struct VulkanDevice;
    struct VulkanBuffer;

    bool create_buffer(const VulkanDevice &device,
                       VkBufferUsageFlags usage_flags,
                       VkMemoryPropertyFlags memory_property_flags,
                       u32 size,
                       VulkanBuffer &out_buffer);
    void destroy_buffer(const VulkanDevice &device, VulkanBuffer &out_buffer);

    void buffer_fill_data(const VulkanDevice &device, const VulkanBuffer &buffer, void *data, u32 size);

    void buffer_copy(const VulkanDevice& device, VulkanBuffer &src, VulkanBuffer &dst, u32 size);
}