#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <array>
#include <core/Logger.h>
#include <core/Memory.h>
#include <rendering/RenderType.h>

#include "vulkan_descriptor.h"
#include "vulkan_pipeline.h"
#include "vulkan_types.h"


namespace hive::vk
{
    bool create_graphics_pipeline(const VulkanDevice &device,
                                  const VkRenderPass &render_pass,
                                  VkPipelineShaderStageCreateInfo *stages,
                                  u32 stages_count,
                                  u32 frame_in_flight,
                                  VkPolygonMode mode,
                                  VulkanPipeline &pipeline)
    {
        //Descriptor
        pipeline.pool = VulkanDescriptorPool::Builder(device)
                .setMaxtSets(frame_in_flight)
                .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame_in_flight)
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frame_in_flight)
                .build();

        if(!pipeline.pool->isReady())
        {
            return false;
        }

        pipeline.layout = VulkanDescriptorSetLayout::Builder(device)
                .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1)
                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                .build();

        pipeline.descriptor_sets.resize(frame_in_flight);
        for(i32 i = 0; i < frame_in_flight; i++)
        {
            pipeline.pool->allocateDescriptor(pipeline.layout->getDescriptorSetLayout(), pipeline.descriptor_sets[i]);
        }



        //Vertex Input
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        //This describe how the data of the vertex is ordered for the GPU
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = mode;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();



        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &pipeline.layout->getDescriptorSetLayout();
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(device.logical_device, &pipelineLayoutInfo, nullptr, &pipeline.pipeline_layout) != VK_SUCCESS) {
            LOG_ERROR("Vulkan: failed to create pipeline layout!");
            return false;
        }


        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.minDepthBounds = 0.0f;
        depth_stencil.maxDepthBounds = 1.0f;
        depth_stencil.stencilTestEnable = VK_TRUE;
        depth_stencil.front = {};
        depth_stencil.back = {};

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = stages_count;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipeline.pipeline_layout;
        pipelineInfo.renderPass = render_pass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDepthStencilState = &depth_stencil;

        if (vkCreateGraphicsPipelines(device.logical_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.vk_pipeline) != VK_SUCCESS) {
            LOG_ERROR("Vulkan: failed to create graphics pipeline!");
            return false;
        }

        return true;
    }

    void destroy_graphics_pipeline(const VulkanDevice &device, VulkanPipeline &pipeline)
    {
        Memory::destroyObject<VulkanDescriptorPool, Memory::RENDERER>(pipeline.pool);
        Memory::destroyObject<VulkanDescriptorSetLayout, Memory::RENDERER>(pipeline.layout);
        vkDestroyPipelineLayout(device.logical_device, pipeline.pipeline_layout, nullptr);
        vkDestroyPipeline(device.logical_device, pipeline.vk_pipeline, nullptr);

        pipeline.vk_pipeline = VK_NULL_HANDLE;
        pipeline.layout = nullptr;
        pipeline.pool = nullptr;
    }

    void pipeline_update_texture_buffer(const VulkanDevice &device, VulkanPipeline &pipeline, u32 frame_in_flight,
        VulkanImage *images)
    {
        for (i32 i = 0; i < frame_in_flight; i++)
        {
            pipeline.texture_buffers = images;
            VkDescriptorImageInfo image_info{};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = pipeline.texture_buffers[0].vk_image_view;
            image_info.sampler = pipeline.texture_buffers[0].vk_sampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = pipeline.descriptor_sets[i];
            descriptorWrite.dstBinding = 1;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = nullptr;
            descriptorWrite.pImageInfo = &image_info; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            vkUpdateDescriptorSets(device.logical_device, 1, &descriptorWrite, 0,
                                   nullptr);
        }
    }


    void pipeline_update_ubo_buffer(const VulkanDevice &device, VulkanPipeline &pipeline, u32 frame_in_flight,
                                    VulkanBuffer *buffers)
    {
        pipeline.ubos = buffers;
        for(i32 i = 0; i < frame_in_flight; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = pipeline.ubos[i].vk_buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = pipeline.descriptor_sets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            vkUpdateDescriptorSets(device.logical_device, 1, &descriptorWrite, 0,
                                   nullptr);
        }
    }
}
#endif
