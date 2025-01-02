#pragma once
#include <rendering/RenderType.h>

namespace hive::vk
{
    struct VulkanSwapchain;
}

namespace hive::vk
{
    struct VulkanDevice;
}

namespace hive::vk
{
    ShaderProgramHandle create_shader_program(const char* vertex_path, const char* fragment_path, const VulkanDevice& vulkan_device, const VulkanSwapchain& swapchain);

    void destroy_program(ShaderProgramHandle shader);

    void use_program(ShaderProgramHandle shader);
}
