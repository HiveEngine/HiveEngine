#pragma once

#include <vulkan/vulkan.h>
namespace hive::vk
{

    struct VulkanDevice;

    bool create_semaphore(const VulkanDevice& device, VkSemaphore *semaphores, u32 count);
    bool create_fence(const VulkanDevice& device, VkFence *fences, u32 count, bool is_signaled);
}