
#include "vulkan_framebuffer.h"
#include "vulkan_image.h"
#include "vulkan_renderpass.h"
#include "vulkan_shader.h"
#include "vulkan_swapchain.h"
#include "rendering/RendererPlatform.h"

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <core/Window.h>
#include <unordered_set>
#include <string>
#include "vulkan/vulkan.h"
#include <vector>

#include "Renderer_Vulkan.h"
#include <core/Logger.h>
#include "vulkan_device.h"


hive::vk::RendererVulkan::RendererVulkan(const Window &window)
{
    LOG_INFO("Initializing Vulkan renderer");
    if (!createInstance(window)) return;
    if (!createSurface(window)) return;
    if (!pickPhysicalDevice()) return;
    if (!createLogicalDevice()) return;
    if (!createSwapChain(window)) return;
    if (!createImageView()) return;
    if (!createRenderPass()) return;
    if (!createFramebuffer()) return;
    if (!createCommandPool()) return;
    if (!createCommandBuffer()) return;
    if (!createSyncObject()) return;

    //Default shader
    ShaderProgramHandle shader = createProgram("shaders/vert.spv", "shaders/frag.spv");
    if (shader.id == -1) return;

    is_ready_ = true;
}

hive::vk::RendererVulkan::~RendererVulkan()
{
    LOG_INFO("Shutting down Vulkan renderer");

    vkDestroyFence(device_.device_, in_flight_fence_, nullptr);
    vkDestroySemaphore(device_.device_, image_available_semaphore_, nullptr);
    vkDestroySemaphore(device_.device_, render_finished_semaphore_, nullptr);


    vkDestroyCommandPool(device_.device_, command_pool_, nullptr);

    for(const auto framebuffer : framebuffers_)
    {
        destroy_framebuffer(device_, framebuffer);
    }
    framebuffers_.clear();

    vkDestroyDevice(device_.device_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}

bool hive::vk::RendererVulkan::isReady() const
{
    return is_ready_;
}

void hive::vk::RendererVulkan::beginDrawing()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(command_buffer_, &beginInfo) != VK_SUCCESS) {
        return;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = render_pass_;
    renderPassInfo.framebuffer = framebuffers_[0];

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain_.extent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(command_buffer_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, shaders_.getData(0).pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_.extent.width);
    viewport.height = static_cast<float>(swapchain_.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer_, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_.extent;
    vkCmdSetScissor(command_buffer_, 0, 1, &scissor);

    vkCmdDraw(command_buffer_, 3, 1, 0, 0);
    vkCmdEndRenderPass(command_buffer_);

    if (vkEndCommandBuffer(command_buffer_) != VK_SUCCESS) {
    }


}



void hive::vk::RendererVulkan::endDrawing()
{
}

void hive::vk::RendererVulkan::frame()
{

    vkWaitForFences(device_.device_, 1, &in_flight_fence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_.device_, 1, &in_flight_fence_);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device_.device_, swapchain_.swapchain_khr, UINT64_MAX, image_available_semaphore_, VK_NULL_HANDLE, &imageIndex);


    vkResetCommandBuffer(command_buffer_, 0);

    beginDrawing();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {image_available_semaphore_};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer_;



    VkSemaphore signalSemaphores[] = {render_finished_semaphore_};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(device_.graphicsQueue_, 1, &submitInfo, in_flight_fence_) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit queue");
    }

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;


    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;

    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain_.swapchain_khr};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pResults = nullptr; // Optional

    vkQueuePresentKHR(device_.present_queue_, &presentInfo);

}


hive::ShaderProgramHandle hive::vk::RendererVulkan::createProgram(const char *vertex_path, const char *fragment_path)
{
    return create_shader_program(vertex_path, fragment_path, device_, swapchain_, render_pass_, shaders_);
}

void hive::vk::RendererVulkan::destroyProgram(const ShaderProgramHandle shader)
{
    destroy_program(shader, device_, shaders_);
}

void hive::vk::RendererVulkan::useProgram(ShaderProgramHandle shader)
{
    use_program(shader);
}

bool hive::vk::RendererVulkan::createInstance(const Window& window)
{
    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "Hive";
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "Hive";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;

    std::vector<const char*> extensions;
    window.appendRequiredVulkanExtension(extensions);

    //TODO: validation layer if debug build

    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
    if(result != VK_SUCCESS)
    {
        return false;
    }

    return true;
}


bool hive::vk::RendererVulkan::createSurface(const Window &window)
{
    window.createVulkanSurface(instance_, &surface_);
    if (surface_ == VK_NULL_HANDLE) return false;

    return true;
}

bool hive::vk::RendererVulkan::pickPhysicalDevice()
{
    PhysicalDeviceRequirements requirements{};
    requirements.graphics = true;
    requirements.extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    pick_physical_device(requirements, device_, surface_, instance_);
    if (device_.physicalDevice_ == VK_NULL_HANDLE)
    {
        LOG_ERROR("Failed to find a suitable physical device");
        return false;
    }

    return true;
}

bool hive::vk::RendererVulkan::createLogicalDevice()
{
    create_logical_device(instance_, device_);

    if(device_.device_ == VK_NULL_HANDLE) return false;

    return true;
}

bool hive::vk::RendererVulkan::createSwapChain(const Window& window)
{
    create_swapchain(instance_, surface_, device_, swapchain_, window);

    if(swapchain_.swapchain_khr == VK_NULL_HANDLE)
    {
        LOG_ERROR("Failed to create a swapchain");
        return false;
    }
    return true;
}

bool hive::vk::RendererVulkan::createImageView()
{
    create_image_view(device_, swapchain_);

    if(swapchain_.image_view_count == -1)
    {
        LOG_ERROR("Failed to create image view");
        return false;
    }

    return true;
}

bool hive::vk::RendererVulkan::createRenderPass()
{
    return create_renderpass(device_, swapchain_, render_pass_);
}

bool hive::vk::RendererVulkan::createFramebuffer()
{
    return create_framebuffer(framebuffers_, device_, swapchain_, render_pass_);
}

bool hive::vk::RendererVulkan::createCommandPool()
{

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = device_.graphics_queue_index_;

    if (vkCreateCommandPool(device_.device_, &poolInfo, nullptr, &command_pool_) != VK_SUCCESS) {
        LOG_ERROR("Failed to create command pool");
        return false;
    }

    return true;
}

bool hive::vk::RendererVulkan::createCommandBuffer()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device_.device_, &allocInfo, &command_buffer_) != VK_SUCCESS) {
        LOG_ERROR("Failed to create command buffer");
        return false;
    }

    return true;
}

bool hive::vk::RendererVulkan::createSyncObject()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(device_.device_, &semaphoreInfo, nullptr, &image_available_semaphore_) != VK_SUCCESS ||
    vkCreateSemaphore(device_.device_, &semaphoreInfo, nullptr, &render_finished_semaphore_) != VK_SUCCESS ||
    vkCreateFence(device_.device_, &fenceInfo, nullptr, &in_flight_fence_) != VK_SUCCESS) {
        LOG_ERROR("Failed to create sync object");
        return false;
    }

    return true;
}


#endif
