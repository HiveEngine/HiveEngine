#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <glm/glm.hpp>
namespace hive::vk
{
    struct Shader
    {
        std::string vertex_path;
        std::string fragment_path;

        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    void create_shader(const VkDevice& device, const VkRenderPass &render_pass, Shader &shader);

    void destroy_shader(const VkDevice &device, const Shader &shader);


    // VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code);
    // void destroy_shader_module(VkDevice device, VkShaderModule module);



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