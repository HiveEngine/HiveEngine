#pragma once
#include <vulkan/vulkan.h>

namespace hive::vk
{
    struct VulkanDevice;
}

namespace hive::vk
{
    void create_renderpass(const VulkanDevice& device, VkRenderPass& renderpass);
    void destroy_renderpass(const VulkanDevice& device, VkRenderPass& renderpass);
}
