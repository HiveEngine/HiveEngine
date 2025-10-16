#include <swarm/precomp.h>
#include <swarm/texture.h>
#include <swarm/device.h>

#include <swarm/vulkan/utils_vulkan.h>

namespace swarm
{
    void CreateDepthTexture(TextureDescription description, VkDevice device, VkPhysicalDevice physicalDevice,
                            VkImage *image, VkImageView *imageView, VkDeviceMemory
                            *deviceMemory);

    void CreateColorTexture(TextureDescription description, VkDevice device, VkImage *image, VkImageView *imageView);

    Texture::Texture(Device &device, TextureDescription description) : m_Image(VK_NULL_HANDLE),
                                                                       m_ImageView(VK_NULL_HANDLE),
                                                                       m_Memory(VK_NULL_HANDLE),
                                                                       m_Device(device)
    {
        switch (description.type)
        {
            case TextureType::COLOR:
                CreateColorTexture(description, m_Device, &m_Image, &m_ImageView);
                break;
            case TextureType::DEPTH:
                CreateDepthTexture(description, m_Device, m_Device.GetPhysicalDevice(), &m_Image, &m_ImageView,
                                   &m_Memory);
                break;
        }
    }

    Texture::~Texture()
    {
        if (m_Image != VK_NULL_HANDLE)
            vkDestroyImage(m_Device, m_Image, nullptr);

        if (m_ImageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_Device, m_ImageView, nullptr);

        if (m_Memory != VK_NULL_HANDLE)
            vkFreeMemory(m_Device, m_Memory, nullptr);
    }


    void CreateDepthTexture(TextureDescription description, VkDevice device, VkPhysicalDevice physicalDevice,
                            VkImage *image, VkImageView *imageView, VkDeviceMemory
                            *deviceMemory)
    {
        VkFormat depthFormat = vk::FindDepthFormat(physicalDevice);

        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = description.width;
        imageCreateInfo.extent.height = description.height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.format = depthFormat;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageCreateInfo, nullptr, image) != VK_SUCCESS)
        {
            //TODO log error
            return;
        }

        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(device, *image, &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = vk::FindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, deviceMemory) != VK_SUCCESS)
        {
            //TODO log error
            return;
        }

        vkBindImageMemory(device, *image, *deviceMemory, 0);

        //======================= VkImageView
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = *image;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = depthFormat;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, imageView) != VK_SUCCESS)
        {
            //TODO log error
            return;
        }
    }

    void CreateColorTexture(TextureDescription description, VkDevice device, VkImage *image, VkImageView *imageView)
    {
    }
}
