#pragma once
#include <rendering/RenderType.h>
#include <vulkan/vulkan_core.h>

#include <core/RessourceManager.h>
namespace hive::vk
{
    struct VulkanSwapchain;
    struct VulkanShader;
    struct VulkanDevice;

    ShaderProgramHandle create_shader_program(const char* vertex_path, const char* fragment_path, const VulkanDevice& vulkan_device, const VulkanSwapchain& swapchain, const VkRenderPass& render_pass, RessourceManager<VulkanShader>& shaders_manager);

    void destroy_program(ShaderProgramHandle shader, const VulkanDevice &device,
                         RessourceManager<VulkanShader> &shader_manager);

    void use_program(ShaderProgramHandle shader);
}
