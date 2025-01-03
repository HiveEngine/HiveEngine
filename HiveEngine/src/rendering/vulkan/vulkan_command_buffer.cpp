#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_command_buffer.h"
#include "vulkan_device.h"
#include <core/Logger.h>

void hive::vk::create_command_pool(Device &device, const VkSurfaceKHR &surface_khr)
{
    VKQueueFamilyIndices queueFamilyIndices = findQueueFamilies(device.physical_device, surface_khr);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device.logical_device, &poolInfo, nullptr, &device.graphics_command_pool) != VK_SUCCESS)
    {
        LOG_ERROR("failed to create command pool!");
    }
}

void hive::vk::create_command_buffer(const Device &device, VkCommandBuffer& command_buffer)
{
    create_command_buffer(device, &command_buffer, 1);
}

void hive::vk::create_command_buffer(const Device &device, VkCommandBuffer* command_buffer, i32 count)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = device.graphics_command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;


    if (vkAllocateCommandBuffers(device.logical_device, &allocInfo, command_buffer) != VK_SUCCESS)
    {
        LOG_ERROR("failed to allocate command buffers!");
    }
}

#endif
