#include <core/Memory.h>
#include <rendering/RendererPlatform.h>

#include "vulkan_descriptor.h"


#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "VkRenderer.h"
#include <core/Logger.h>

#include "vulkan_init.h"
#include "vulkan_debug.h"
#include "vulkan_swapchain.h"
#include "vulkan_device.h"
#include "vulkan_renderpass.h"
#include "vulkan_command.h"
#include "vulkan_sync.h"
#include "vulkan_shader.h"
#include "vulkan_pipeline.h"
#include "vulkan_buffer.h"
#include "vulkan_image.h"

std::vector<hive::vk::Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};

std::vector<u16> indices = {
    0, 1, 2, 2, 3, 0
};


hive::vk::VkRenderer::VkRenderer(const Window& window) : shaders_manager_(16)
{
    if(!create_instance(instance_, window)) return;
    if(config::enable_validation && !setup_debug_messenger(instance_, debugMessenger)) return;
    if(!create_surface(instance_, window, surface_khr_)) return;
    if(!create_device(instance_, surface_khr_, device_)) return;
    if(!create_swapchain(device_, surface_khr_, window, swapchain_)) return;
    if(!create_renderpass(device_, swapchain_, render_pass_)) return;
    if(!create_framebuffer(device_, swapchain_, render_pass_, framebuffer_)) return;
    if(!create_command_pool(device_)) return;
    if(!create_command_buffer(device_, command_buffers_, MAX_FRAME_IN_FLIGHT)) return;

    if (!create_semaphore(device_, sem_image_available_, MAX_FRAME_IN_FLIGHT)
        || !create_semaphore(device_, sem_render_finished_, MAX_FRAME_IN_FLIGHT)
        || !create_fence(device_, fence_in_flight_, MAX_FRAME_IN_FLIGHT, true)) return;

    //the rest is temporary
    {
        //Create texture
        i32 texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        u32 image_size = texWidth * texHeight * 4;

        if(!pixels)
        {
            return;
        }

        VulkanBuffer temp_texture_buffer;
        create_buffer(device_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, image_size, temp_texture_buffer);
        buffer_fill_data(device_, temp_texture_buffer, pixels, image_size);
        stbi_image_free(pixels);

        VulkanImage image;
        create_image(device_, texWidth, texHeight,
                     VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     image);
        transition_image_layout(device_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image);
            copy_buffer_to_image(device_, temp_texture_buffer, image, texWidth, texHeight);
        transition_image_layout(device_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, image);
        destroy_buffer(device_, temp_texture_buffer);
        create_image_view(device_, image.vk_image, image.vk_image_view);
        create_image_sampler(device_, image.vk_sampler);


        // destroy_image(device_, image);

        //Vertex buffer
        VulkanBuffer temp_vertex_buffer;
        create_buffer(device_,  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertices.size() * sizeof(vertices[0]), temp_vertex_buffer);
        buffer_fill_data(device_, temp_vertex_buffer, vertices.data(), vertices.size() * sizeof(vertices[0]));
        create_buffer(device_, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertices.size() * sizeof(vertices[0]), vertex_buffer_);
        buffer_copy(device_, temp_vertex_buffer, vertex_buffer_, vertices.size() * sizeof(vertices[0]));
        destroy_buffer(device_, temp_vertex_buffer);

        //Index buffer
        VulkanBuffer temp_index_buffer;
        create_buffer(device_,  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indices.size() * sizeof(indices[0]), temp_index_buffer);
        buffer_fill_data(device_, temp_index_buffer, indices.data(), indices.size() * sizeof(indices[0]));
        create_buffer(device_,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indices.size() * sizeof(indices[0]), index_buffer_);
        buffer_copy(device_, temp_index_buffer, index_buffer_, indices.size() * sizeof(indices[0]));
        destroy_buffer(device_, temp_index_buffer);


        //UBO
        for(auto & ubo : ubos)
        {
            create_buffer(device_, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(UniformBufferObject), ubo);
        }


        // DescriptorPool
        descriptor_pool_ = VulkanDescriptorPool::Builder(device_)
                 .setMaxtSets(MAX_FRAME_IN_FLIGHT)
                 .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAME_IN_FLIGHT)
                 .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAME_IN_FLIGHT)
                 .build();

        if (!descriptor_pool_->isReady())
        {
            return;
        }


        //DescriptorLayout
        VulkanDescriptorSetLayout *layout = VulkanDescriptorSetLayout::Builder(device_)
                .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1)
                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                .build();


        descriptorSets_.resize(MAX_FRAME_IN_FLIGHT);

        descriptor_pool_->allocateDescriptor(layout->getDescriptorSetLayout(), descriptorSets_[0]);
        descriptor_pool_->allocateDescriptor(layout->getDescriptorSetLayout(), descriptorSets_[1]);


        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = ubos[i].vk_buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo image_info{};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = image.vk_image_view;
            image_info.sampler = image.vk_sampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets_[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;
            descriptorWrites[0].pImageInfo = nullptr; // Optional
            descriptorWrites[0].pTexelBufferView = nullptr; // Optional

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets_[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = nullptr;
            descriptorWrites[1].pImageInfo = &image_info; // Optional
            descriptorWrites[1].pTexelBufferView = nullptr; // Optional

            vkUpdateDescriptorSets(device_.logical_device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
        }

        //Shader
        VkShaderModule vert_module;
        VkShaderModule frag_module;
        if (!create_shader_module(device_, "shaders/vert.spv", vert_module)) return;
        if (!create_shader_module(device_, "shaders/frag.spv", frag_module)) return;

        auto vert_stage = create_stage_info(vert_module, StageType::VERTEX);
        auto frag_stage = create_stage_info(frag_module, StageType::FRAGMENT);

        VkPipelineShaderStageCreateInfo stages[] = {vert_stage, frag_stage};

        if (!create_graphics_pipeline(device_, render_pass_, stages, 2, layout, default_pipeline_ )) return;

        destroy_shader_module(device_, vert_module);
        destroy_shader_module(device_, frag_module);
    }

    is_ready_ = true;
}

hive::vk::VkRenderer::~VkRenderer()
{
    vkDeviceWaitIdle(device_.logical_device);
    //Temporary
    {
        destroy_buffer(device_, vertex_buffer_);
        destroy_buffer(device_, index_buffer_);

        for(auto buffer : ubos)
        {
            destroy_buffer(device_, buffer);
        }

        Memory::destroyObject<VulkanDescriptorPool, Memory::RENDERER>(descriptor_pool_);

        destroy_graphics_pipeline(device_, default_pipeline_);
    }

    destroy_swapchain(device_, swapchain_);
    destroy_renderpass(device_, render_pass_);
    destroy_framebuffer(device_, framebuffer_);
    destroy_semaphores(device_, sem_image_available_, MAX_FRAME_IN_FLIGHT);
    destroy_semaphores(device_, sem_render_finished_, MAX_FRAME_IN_FLIGHT);
    destroy_fences(device_, fence_in_flight_, MAX_FRAME_IN_FLIGHT);
    destroy_command_pool(device_);

    //debug messenger if in debug mode
    if(config::enable_validation)
    {
        destroy_debug_util_mesenger(instance_, debugMessenger);
    }

    destroy_surface(instance_, surface_khr_);
    destroy_device(device_);
    destroy_instance(instance_);
}


bool hive::vk::VkRenderer::isReady() const
{
    return is_ready_;
}

void hive::vk::VkRenderer::recordCommandBuffer(VkCommandBuffer command_buffer)
{
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, default_pipeline_.vk_pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchain_.extent_2d.width;
    viewport.height = (float) swapchain_.extent_2d.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_.extent_2d;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {vertex_buffer_.vk_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(command_buffer, index_buffer_.vk_buffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, default_pipeline_.pipeline_layout, 0, 1, &descriptorSets_[current_frame_], 0, nullptr);
    vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

}

void hive::vk::VkRenderer::updateUniformBuffer()
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swapchain_.extent_2d.width / (float) swapchain_.extent_2d.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(ubos[current_frame_].map, &ubo, sizeof(ubo));


}

void hive::vk::VkRenderer::temp_draw()
{
    updateUniformBuffer();
    recordCommandBuffer(command_buffers_[current_frame_]);
}

bool hive::vk::VkRenderer::beginDrawing()
{
    vkWaitForFences(device_.logical_device, 1, &fence_in_flight_[current_frame_], VK_TRUE, UINT64_MAX);
    vkResetFences(device_.logical_device, 1, &fence_in_flight_[current_frame_]);

    vkAcquireNextImageKHR(device_.logical_device, swapchain_.vk_swapchain, UINT64_MAX, sem_image_available_[current_frame_], VK_NULL_HANDLE, &image_index_);

    vkResetCommandBuffer(command_buffers_[current_frame_], /*VkCommandBufferResetFlagBits*/ 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(command_buffers_[current_frame_], &beginInfo) != VK_SUCCESS) {
        LOG_ERROR("Vulkan: failed to begin recording command buffer!");
        return false;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = render_pass_;
    renderPassInfo.framebuffer = framebuffer_.framebuffers[image_index_];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain_.extent_2d;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;


    vkCmdBeginRenderPass(command_buffers_[current_frame_], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    return true;
}

bool hive::vk::VkRenderer::endDrawing()
{
    vkCmdEndRenderPass(command_buffers_[current_frame_]);

    if (vkEndCommandBuffer(command_buffers_[current_frame_]) != VK_SUCCESS) {
        LOG_ERROR("Vulkan: failed to record command buffer!");
        return false;
    }
    return true;
}

bool hive::vk::VkRenderer::frame()
{
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {sem_image_available_[current_frame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffers_[current_frame_];

    VkSemaphore signalSemaphores[] = {sem_render_finished_[current_frame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(device_.graphics_queue, 1, &submitInfo, fence_in_flight_[current_frame_]) != VK_SUCCESS)
    {
        LOG_ERROR("Vulkan: failed to submit draw command buffer!");
        return false;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain_.vk_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &image_index_;

    vkQueuePresentKHR(device_.present_queue, &presentInfo);

    current_frame_ = (current_frame_ + 1) % MAX_FRAME_IN_FLIGHT;

    return true;
}

//TODO: use something like spirv-cross to get the layout or provide a structure that is passed to the function in order to tell the glsl layout
hive::ShaderProgramHandle hive::vk::VkRenderer::createShader(const char *vertex_path, const char *fragment_path)
{

    auto shader = shaders_manager_.getAvailableId();
    auto& shader_data = shaders_manager_.getData(shader);

    VkShaderModule vert_module;
    VkShaderModule frag_module;
    if (!create_shader_module(device_, vertex_path, vert_module)) return {};
    if (!create_shader_module(device_, fragment_path, frag_module)) return {};

    auto vert_stage = create_stage_info(vert_module, StageType::VERTEX);
    auto frag_stage = create_stage_info(frag_module, StageType::FRAGMENT);

    VkPipelineShaderStageCreateInfo stages[] = {vert_stage, frag_stage};

    // if (!create_graphics_pipeline(device_, render_pass_, stages, 2, shader_data)) return {};

    destroy_shader_module(device_, vert_module);
    destroy_shader_module(device_, frag_module);

    return shader;
}

void hive::vk::VkRenderer::destroyShader(hive::ShaderProgramHandle shader)
{
    //TODO: find a way to make sure the shader is not in used
    auto& shader_data = shaders_manager_.getData(shader);
    destroy_graphics_pipeline(device_, shader_data);
}

void hive::vk::VkRenderer::useShader(ShaderProgramHandle shader)
{
    auto &shader_data = shaders_manager_.getData(shader);
    vkCmdBindPipeline(command_buffers_[current_frame_], VK_PIPELINE_BIND_POINT_GRAPHICS, shader_data.vk_pipeline);
}


#endif
