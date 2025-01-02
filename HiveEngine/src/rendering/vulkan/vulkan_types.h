#pragma once

#include <vector>
#include <vulkan/vulkan.h>
namespace hive::vk
{
    struct PhysicalDeviceRequirements
    {
        bool graphics;
        bool presentation;
        bool compute;
        bool transfer;
        bool discrete_gpu;


        std::vector<const char *> extensions;
        //... other
    };

    struct PhysicalDeviceFamilyQueueInfo
    {
        i32 graphics_family_index;
        i32 present_family_index;
        i32 compute_family_index;
        i32 transfer_family_index;
    };

    struct VulkanDevice
    {
        VkPhysicalDevice physicalDevice_{};
        VkDevice device_{};

        i32 graphics_queue_index_{};
        i32 transfer_queue_index_{};
        i32 present_queue_index_{};

        VkQueue graphicsQueue_{};
        VkQueue present_queue_{};
    };

    struct VulkanSwapchain
    {
        VkSwapchainKHR swapchain_khr;

        VkImage images[32];
        u32 image_count = 0;

        VkImageView image_view[32];
        u32 image_view_count = 0;

        VkFormat format;
        VkExtent2D extent;
    };

    struct VulkanShader
    {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };


}