#pragma once

namespace hive::vk
{
    bool create_command_pool(VulkanDevice &device);
    bool create_command_buffer(const VulkanDevice& device, VkCommandBuffer *buffers, u32 count);

    void destroy_command_pool(VulkanDevice &device);
}