#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>


namespace hive::vk
{
    struct VulkanDevice;
    struct VulkanPipeline;
    struct VulkanImage;
    struct VulkanBuffer;

    bool create_graphics_pipeline(const VulkanDevice &device,
                                  const VkRenderPass &render_pass,
                                  VkPipelineShaderStageCreateInfo *stages,
                                  u32 stages_count,
                                  u32 frame_in_flight,
                                  VkPolygonMode mode,
                                  VulkanPipeline &pipeline);

    void destroy_graphics_pipeline(const VulkanDevice &device, VulkanPipeline &pipeline);

    void pipeline_update_texture_buffer(const VulkanDevice &device, VulkanPipeline &pipeline, u32 frame_in_flight,
                                        VulkanImage *images);

    void pipeline_update_ubo_buffer(const VulkanDevice &device, VulkanPipeline &pipeline, u32 frame_in_flight,
                                    VulkanBuffer *buffers);





}
