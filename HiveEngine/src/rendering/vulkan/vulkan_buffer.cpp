#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_device.h"
#include <core/Logger.h>
#include "vulkan_buffer.h"
#include <vulkan/vulkan.h>
#include <cstring>

namespace hive::vk
{
    void create_vertex_buffer(const Device& device, u32 size, Buffer& buffer);
    u32 findMemoryType(const Device& device, u32 filterType, VkMemoryPropertyFlags properties);

    void create_buffer(const Device& device, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer, u32 size)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device.logical_device, &bufferInfo, nullptr, &buffer.vk_buffer) != VK_SUCCESS)
        {
            LOG_ERROR("failed to create buffer!");
            return;
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device.logical_device, buffer.vk_buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(device, memRequirements.memoryTypeBits, properties);

        //TODO: GPU have a maximum allocation count that is very limited (4096)
        //so we need to lower the allocation count by splitting allocation for multiple object with the use of offset
        // See https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator for more detail
        if (vkAllocateMemory(device.logical_device, &allocInfo, nullptr, &buffer.vk_buffer_memory) != VK_SUCCESS)
        {
            LOG_ERROR("failed to allocate buffer memory!");
            return;
        }

        vkBindBufferMemory(device.logical_device, buffer.vk_buffer, buffer.vk_buffer_memory, 0);
    }

    void fill_buffer_data(const Device &device, const Buffer &buffer, void *data, u32 size)
    {
        void* local_data;
        vkMapMemory(device.logical_device, buffer.vk_buffer_memory, 0, size, 0, &local_data);
        std::memcpy(local_data, data, size);
        vkUnmapMemory(device.logical_device, buffer.vk_buffer_memory);
    }

    void copy_buffer_data(const Device& device, Buffer& src_buffer, Buffer& dst_buffer, u32 size)
    {
        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandPool = device.graphics_command_pool;
        allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(device.logical_device, &allocate_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);

        VkBufferCopy copy_region{};
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = size;

        vkCmdCopyBuffer(command_buffer, src_buffer.vk_buffer, dst_buffer.vk_buffer, 1, &copy_region);
        vkEndCommandBuffer(command_buffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &command_buffer;

        vkQueueSubmit(device.graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(device.graphics_queue);

        vkFreeCommandBuffers(device.logical_device, device.graphics_command_pool, 1, &command_buffer);

    }

    void destroy_buffer(const Device &device, const Buffer &buffer)
    {
        vkDestroyBuffer(device.logical_device, buffer.vk_buffer, nullptr);
        vkFreeMemory(device.logical_device, buffer.vk_buffer_memory, nullptr);
    }


    void create_vertex_buffer(const Device& device, u32 size, Buffer& buffer)
    {
        VkBufferCreateInfo buffer_create_info{};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = size;
        buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if(vkCreateBuffer(device.logical_device, &buffer_create_info, nullptr, &buffer.vk_buffer) != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create vertex buffer");
        }

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device.logical_device, buffer.vk_buffer, &memory_requirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memory_requirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device.logical_device, &allocInfo, nullptr, &buffer.vk_buffer_memory) != VK_SUCCESS) {
            LOG_ERROR("failed to allocate vertex buffer memory!");
            return;
        }

        vkBindBufferMemory(device.logical_device, buffer.vk_buffer, buffer.vk_buffer_memory, 0);
        buffer.buffer_info = buffer_create_info;

        // void* data;
        // vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);
        // memcpy(data, vertices.data(), (size_t) bufferInfo.size);
        // vkUnmapMemory(device, vertexBufferMemory);

    }

    u32 findMemoryType(const Device& device, u32 filterType, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(device.physical_device, &memory_properties);

        for(u32 i = 0; i < memory_properties.memoryTypeCount; i++)
        {
            if(filterType & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        LOG_ERROR("Failed to find suitable memory type");
        return U32_MAX;
    }

}
#endif
