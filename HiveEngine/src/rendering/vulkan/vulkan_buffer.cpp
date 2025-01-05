#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <vulkan/vulkan.h>
#include <cstring>

#include <core/Logger.h>

#include "vulkan_types.h"
#include "vulkan_buffer.h"
namespace hive::vk
{
    bool findMemoryType(const VulkanDevice &device, u32 typeFilter, VkMemoryPropertyFlags properties, u32 &out_index);


    bool create_buffer(const VulkanDevice &device, VkBufferUsageFlags usage_flags, u32 size, VulkanBuffer &out_buffer)
    {
        VkBufferCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        create_info.size = size;
        create_info.usage = usage_flags;
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device.logical_device, &create_info, nullptr, &out_buffer.vk_buffer) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create buffer");
            return false;
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device.logical_device, out_buffer.vk_buffer, &memRequirements);

        u32 memory_type_index;
        if (!findMemoryType(device, memRequirements.memoryTypeBits,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           memory_type_index))
        {
            LOG_ERROR("Vulkan: failed to find visible memory type");
            return false;
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memory_type_index;


        if (vkAllocateMemory(device.logical_device, &allocInfo, nullptr, &out_buffer.vk_buffer_memory) != VK_SUCCESS) {
            destroy_buffer(device, out_buffer);
            LOG_ERROR("Vulkan: failed to allocate vertex buffer memory!");
            return false;
        }

        vkBindBufferMemory(device.logical_device, out_buffer.vk_buffer, out_buffer.vk_buffer_memory, 0);

        return true;
    }

    void destroy_buffer(const VulkanDevice &device, VulkanBuffer &out_buffer)
    {
        vkFreeMemory(device.logical_device, out_buffer.vk_buffer_memory, nullptr);
        vkDestroyBuffer(device.logical_device, out_buffer.vk_buffer, nullptr);
        out_buffer.vk_buffer = VK_NULL_HANDLE;
    }

    void buffer_fill_data(const VulkanDevice &device, const VulkanBuffer &buffer, void *data, u32 size)
    {
        void* local_data;
        vkMapMemory(device.logical_device, buffer.vk_buffer_memory, 0, size, 0, &local_data);
        memcpy(data, data, size);
        vkUnmapMemory(device.logical_device, buffer.vk_buffer_memory);

    }

    bool findMemoryType(const VulkanDevice &device, u32 typeFilter, VkMemoryPropertyFlags properties, u32 &out_index)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(device.physical_device, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                out_index = i;
                return true;
            }
        }

        return false;
    }
}
#endif
