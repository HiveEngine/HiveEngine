#pragma once

#include <vulkan/vulkan.h>
#include <string>
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
}