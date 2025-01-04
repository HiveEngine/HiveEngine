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

        std::vector<VkImage> images;
        std::vector<VkImageView> image_views;
    };

    struct VulkanFramebuffer
    {
        std::vector<VkFramebuffer> framebuffers;
    };

    struct VulkanPipeline
    {
        VkPipeline vk_pipeline;
        VkPipelineLayout pipeline_layout;

        VkDescriptorSetLayout descriptor_set_layout;
    };

    struct Vertex
    {
        glm::vec2 pos;
        glm::vec3 color;

        static VkVertexInputBindingDescription getBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);
            return attributeDescriptions;
        }
    };
}