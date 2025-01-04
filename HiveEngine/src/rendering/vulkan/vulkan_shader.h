#pragma once
#include <vulkan/vulkan.h>

namespace hive::vk
{
    struct VulkanDevice;

    bool create_shader_module(const VulkanDevice& device, const char* path, VkShaderModule &out_module);

    void destroy_shader_module(const VulkanDevice& device, VkShaderModule &module);


    enum class StageType
    {
        VERTEX, FRAGMENT
    };
    VkPipelineShaderStageCreateInfo create_stage_info(const VkShaderModule& shader_module, StageType type);
}
