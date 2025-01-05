#include <rendering/RendererPlatform.h>


#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "VkRenderer.h"

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
std::vector<hive::vk::Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

std::vector<u16> indices = {
    0, 1, 2, 2, 3, 0
};


hive::vk::VkRenderer::VkRenderer(const Window& window)
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
        VkShaderModule vert_module;
        VkShaderModule frag_module;
        if(!create_shader_module(device_, "shaders/vert.spv", vert_module)) return;
        if(!create_shader_module(device_, "shaders/frag.spv", frag_module)) return;

        auto vert_stage = create_stage_info(vert_module, StageType::VERTEX);
        auto frag_stage = create_stage_info(frag_module, StageType::FRAGMENT);

        VkPipelineShaderStageCreateInfo stages[] = {vert_stage, frag_stage};

        if (!create_graphics_pipeline(device_, render_pass_, stages, 2, default_pipeline_)) return;

        destroy_shader_module(device_, vert_module);
        destroy_shader_module(device_, frag_module);

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

    }

    destroy_swapchain(device_, swapchain_);
    destroy_graphics_pipeline(device_, default_pipeline_);
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

void hive::vk::VkRenderer::recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(command_buffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = render_pass_;
    renderPassInfo.framebuffer = framebuffer_.framebuffers[image_index];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain_.extent_2d;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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
    // vkCmdDraw(command_buffer, 3, 1, 0, 0);
    // vkCmdDraw(command_buffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
    vkCmdDrawIndexed(command_buffer, indices.size(), 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void hive::vk::VkRenderer::temp_draw()
{
    vkWaitForFences(device_.logical_device, 1, &fence_in_flight_[current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(device_.logical_device, 1, &fence_in_flight_[current_frame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device_.logical_device, swapchain_.vk_swapchain, UINT64_MAX, sem_image_available_[current_frame], VK_NULL_HANDLE, &imageIndex);

    vkResetCommandBuffer(command_buffers_[current_frame], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(command_buffers_[current_frame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {sem_image_available_[current_frame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffers_[current_frame];

    VkSemaphore signalSemaphores[] = {sem_render_finished_[current_frame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(device_.graphics_queue, 1, &submitInfo, fence_in_flight_[current_frame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain_.vk_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(device_.present_queue, &presentInfo);

    current_frame = (current_frame + 1) % MAX_FRAME_IN_FLIGHT;
}

bool hive::vk::VkRenderer::beginDrawing()
{
    return true;
}

void hive::vk::VkRenderer::endDrawing()
{
}

void hive::vk::VkRenderer::frame()
{
}

hive::ShaderProgramHandle hive::vk::VkRenderer::createProgram(const char *vertex_path, const char *fragment_path)
{
    return {0};
}

void hive::vk::VkRenderer::destroyProgram(hive::ShaderProgramHandle shader)
{
}

void hive::vk::VkRenderer::useProgram(hive::ShaderProgramHandle shader)
{
}

void hive::vk::VkRenderer::createShaderLayout()
{
}

#endif
