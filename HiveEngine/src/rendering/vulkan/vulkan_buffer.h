#pragma once

namespace hive::vk
{
    struct Device;

    enum class BufferType
    {
        Vertex
    };

    struct Buffer
    {
        VkBuffer vk_buffer;
        VkDeviceMemory vk_buffer_memory;
        VkBufferCreateInfo buffer_info;
        u32 count;
    };

    void create_buffer(const Device& device, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer, u32 size);
    void fill_buffer_data(const Device& device, const Buffer& buffer, void* data, u32 size);
    void copy_buffer_data(const Device& device, Buffer& src_buffer, Buffer& dst_buffer, u32 size);
    void destroy_buffer(const Device& device, const Buffer& buffer);
}