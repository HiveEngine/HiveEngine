#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <core/Logger.h>
#include "vulkan_sync.h"
#include "vulkan_types.h"

#include <vulkan/vulkan.h>
namespace hive::vk
{
    bool create_semaphore(const VulkanDevice& device, VkSemaphore *semaphores, u32 count)
    {
        VkSemaphoreCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for(u32 i = 0; i < count; i++)
        {
            if(vkCreateSemaphore(device.logical_device, &create_info, nullptr, &semaphores[i]) != VK_SUCCESS)
            {
                LOG_ERROR("Vulkan: failed to create semaphore");
                return false;
            }
        }

        return true;
    }
    bool create_fence(const VulkanDevice& device, VkFence *fences, u32 count, bool is_signaled)
    {
        VkFenceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if(is_signaled)
        {
            create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }


        for(u32 i = 0; i < count; i++)
        {
            if (vkCreateFence(device.logical_device, &create_info, nullptr, &fences[i]) != VK_SUCCESS)
            {
                LOG_ERROR("Vulkan: failed to create fence");
                return false;
            }
        }

        return true;
    }

    void destroy_semaphores(const VulkanDevice &device, const VkSemaphore*semaphore, const u32 count)
    {
        for(i32 i = 0; i < count; i++)
        {
            vkDestroySemaphore(device.logical_device, semaphore[i], nullptr);
        }
    }

    void destroy_fences(const VulkanDevice &device, const VkFence *fence, u32 count)
    {
        for (i32 i = 0; i < count; i++)
        {
            vkDestroyFence(device.logical_device, fence[i], nullptr);
        }
    }
}
#endif