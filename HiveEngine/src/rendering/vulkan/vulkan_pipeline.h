#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>

namespace hive::vk
{
    struct VulkanDevice;
    struct VulkanPipeline;


     bool create_graphics_pipeline(const VulkanDevice &device,
                                  const VkRenderPass &render_pass,
                                  VkPipelineShaderStageCreateInfo *stages,
                                  u32 stages_count,
                                  VulkanPipeline &pipeline);

    void destroy_graphics_pipeline(const VulkanDevice &device, VulkanPipeline &pipeline);

    struct Vertex {
        glm::vec2 pos;
        glm::vec3 color;
    };



}
