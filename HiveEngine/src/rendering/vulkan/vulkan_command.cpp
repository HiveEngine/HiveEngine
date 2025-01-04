#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <vulkan/vulkan.h>

#include "vulkan_types.h"
#include "vulkan_command.h"

#include <core/Logger.h>

namespace hive::vk
{
    bool create_command_pool(VulkanDevice &device)
    {
        VkCommandPoolCreateInfo pool_create_info{};
        pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_create_info.queueFamilyIndex = device.graphisc_family_index;

        if(vkCreateCommandPool(device.logical_device, &pool_create_info, nullptr, &device.graphics_command_pool) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create a command pool");
            return false;
        }

        return true;
    }

    bool create_command_buffer(const VulkanDevice& device, VkCommandBuffer *buffers, u32 count)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = device.graphics_command_pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = count;


        if (vkAllocateCommandBuffers(device.logical_device, &allocInfo, buffers) != VK_SUCCESS)
        {
            LOG_ERROR("failed to allocate command buffers!");
            return false;
        }

        return true;
    }

    void destroy_command_pool(VulkanDevice &device)
    {
        vkDestroyCommandPool(device.logical_device, device.graphics_command_pool, nullptr);
        device.graphics_command_pool = VK_NULL_HANDLE;
    }
}
#endif
