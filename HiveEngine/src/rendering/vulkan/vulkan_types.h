#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
namespace hive::vk
{
    struct VulkanDevice
    {
        VkPhysicalDevice physical_device;
        VkDevice logical_device;

        VkQueue graphics_queue;
        VkQueue present_queue;

        u32 graphisc_family_index;
        u32 present_family_index;

        VkCommandPool graphics_command_pool;
    };

    struct VulkanSwapchain
    {
        VkSwapchainKHR vk_swapchain;
        VkFormat image_format;
        VkExtent2D extent_2d;

        //TODO: use vector of VulkanImage instead
        std::vector<VkImage> images;
        std::vector<VkImageView> image_views;
    };

    struct VulkanFramebuffer
    {
        std::vector<VkFramebuffer> framebuffers;
    };


    class VulkanDescriptorSetLayout;
    struct VulkanPipeline
    {
        VkPipeline vk_pipeline;
        VkPipelineLayout pipeline_layout;
        VulkanDescriptorSetLayout *layout;
    };



    struct VulkanBuffer
    {
        VkBuffer vk_buffer;
        VkDeviceMemory vk_buffer_memory;
        void* map;
    };

    struct VulkanImage
    {
        VkImage vk_image;
        VkDeviceMemory vk_image_memory;
        VkImageView vk_image_view;
        VkSampler vk_sampler;
    };

}