#pragma once

#include <vulkan/vulkan.h>
#include <vector>
namespace hive::vk
{
    struct Device;

    void create_command_pool(Device &device, const VkSurfaceKHR &surface_khr);
    void create_command_buffer(const Device &device, VkCommandBuffer &command_buffer);
    void create_command_buffer(const Device &device, VkCommandBuffer* command_buffer, i32 count);
}
