#include <core/Window.h>
#include <core/Logger.h>
#include "VulkanRenderer.h"
#include "vulkan_device.h"
#include "vulkan_shader.h"
#include "vulkan_swapchain.h"

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <iostream>
#include <cstring>
#include <rendering/vulkan/vulkan_renderpass.h>

#include "vulkan_command_buffer.h"
#include "vulkan_framebuffer.h"
#include "vulkan_renderpass.h"


bool checkValidationLayerSupport(const std::vector<const char*> &validationLayers);
std::vector<const char *> getRequiredExtensions(const hive::Window& window, bool enable_validation);


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void *pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

hive::vk::VulkanRenderer::VulkanRenderer(const Window &window) : window_(window)
{

    DeviceConfig config;
    config.enable_validation_layers = true;

    if (!createInstance(window, config)) return;
    if (config.enable_validation_layers && !setupDebugMessenger()) return;
    if (!createSurface(window)) return;
    if (!createDevice()) return;
    if (!createSwapChain(window)) return;
    if (!createRenderPass()) return;
    if (!createFramebuffers()) return;
    if (!createCommandPool()) return;
    if (!createCommandBuffer()) return;
    if (!createSyncObjects()) return;

    is_ready_ = true;
}

hive::vk::VulkanRenderer::~VulkanRenderer()
{

}



bool hive::vk::VulkanRenderer::isReady() const
{
    return is_ready_;
}

#include <glm/glm.hpp>
struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
};

const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

void hive::vk::VulkanRenderer::temp_draw()
{
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(swapchain_.extent_2d.width);
    viewport.height = static_cast<f32>(swapchain_.extent_2d.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffers_[current_frame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_.extent_2d;
    vkCmdSetScissor(command_buffers_[current_frame], 0, 1, &scissor);

    vkCmdDraw(command_buffers_[current_frame], 3, 1, 0, 0);
}

bool hive::vk::VulkanRenderer::beginDrawing()
{
    vkWaitForFences(device_.logical_device, 1, &in_flight_fence_[current_frame], VK_TRUE, UINT64_MAX);
    VkResult result = vkAcquireNextImageKHR(device_.logical_device, swapchain_.swapchain_khr, UINT64_MAX, image_available_semaphore_[current_frame], VK_NULL_HANDLE, &imageIndex);
    if(result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        LOG_ERROR("Failed to acquire swap chain image");
    }

    vkResetFences(device_.logical_device, 1, &in_flight_fence_[current_frame]);


    vkResetCommandBuffer(command_buffers_[current_frame], /*VkCommandBufferResetFlagBits*/ 0);


    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(command_buffers_[current_frame], &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = render_pass_.vk_render_pass;
    renderPassInfo.framebuffer = framebuffer_.vk_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain_.extent_2d;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(command_buffers_[current_frame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    return true;
}

void hive::vk::VulkanRenderer::endDrawing()
{

    vkCmdEndRenderPass(command_buffers_[current_frame]);
    // recordCommandBuffer(command_buffer_, imageIndex);
    if (vkEndCommandBuffer(command_buffers_[current_frame]) != VK_SUCCESS)
    {
        LOG_ERROR("failed to record command buffer!");
    }


}

void hive::vk::VulkanRenderer::frame()
{
   VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {image_available_semaphore_[current_frame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffers_[current_frame];

    VkSemaphore signalSemaphores[] = {render_finished_semaphore_[current_frame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(device_.graphics_queue, 1, &submitInfo, in_flight_fence_[current_frame]) != VK_SUCCESS)
    {
        LOG_ERROR("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain_.swapchain_khr};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(device_.present_queue, &presentInfo);
    current_frame = (current_frame + 1) % MAX_FRAME_IN_FLIGHT;
}

hive::ShaderProgramHandle hive::vk::VulkanRenderer::createProgram(const char *vertex_path, const char *fragment_path)
{
    i32 id = shaders_manager_.getAvailableId();
    if(id == -1)
    {
        Shader shader;
        shader.fragment_path = fragment_path;
        shader.vertex_path = vertex_path;
        create_shader(device_.logical_device, render_pass_.vk_render_pass, shader);
        id = shaders_manager_.pushData(shader);
        return {static_cast<u32>(id)};
    }
    else
    {
        Shader &shader = shaders_manager_.getData(id);
        shader.fragment_path = fragment_path;
        shader.vertex_path = vertex_path;
        create_shader(device_.logical_device, render_pass_.vk_render_pass, shader);
        return {static_cast<u32>(id)};
    }

}

void hive::vk::VulkanRenderer::destroyProgram(const ShaderProgramHandle shader)
{
    destroy_shader(device_.logical_device, shaders_manager_.getData(shader.id));
    shaders_manager_.clearData(shader.id);
}

void hive::vk::VulkanRenderer::useProgram(ShaderProgramHandle shader)
{
    vkCmdBindPipeline(command_buffers_[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, shaders_manager_.getData(shader.id).pipeline);
}

bool hive::vk::VulkanRenderer::createInstance(const Window& window, const DeviceConfig& config)
{
    if (config.enable_validation_layers && !checkValidationLayerSupport(config.validation_layers))
    {
        LOG_ERROR("validation layers requested, but not available!");
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions(window, config.enable_validation_layers);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (config.enable_validation_layers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(config.validation_layers.size());
        createInfo.ppEnabledLayerNames = config.validation_layers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debugCreateInfo;
    } else
    {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS)
    {
        LOG_ERROR("failed to create instance!");
        return false;
    }

    return true;
}

bool hive::vk::VulkanRenderer::setupDebugMessenger()
{

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        LOG_ERROR("Failed to setup debug messenger");
        return false;
    }

    return true;
}

bool hive::vk::VulkanRenderer::createSurface(const Window& window)
{

    window.createVulkanSurface(instance_, &surface_);

    if (surface_ == VK_NULL_HANDLE)
    {
        LOG_ERROR("failed to create window surface!");
        return false;
    }

    return true;
}

bool hive::vk::VulkanRenderer::createDevice()
{

    create_device(instance_, surface_, device_);

    if(device_.logical_device == VK_NULL_HANDLE) return false;

    return true;
}

bool hive::vk::VulkanRenderer::createSwapChain(const Window& window)
{
    create_swapchain(device_, window, surface_, swapchain_);

    if(swapchain_.swapchain_khr == VK_NULL_HANDLE) return false;

    return true;
}

bool hive::vk::VulkanRenderer::createRenderPass()
{
    create_render_pass(device_, swapchain_, render_pass_);

    if(render_pass_.vk_render_pass == VK_NULL_HANDLE) return false;

    return true;
}



bool hive::vk::VulkanRenderer::createFramebuffers()
{
    create_framebuffer(device_, swapchain_, render_pass_, framebuffer_);

    if(framebuffer_.vk_framebuffers.size() != swapchain_.image_views.size()) return false;

    return true;
}

bool hive::vk::VulkanRenderer::createCommandPool()
{
    create_command_pool(device_, surface_);

    if(device_.graphics_command_pool == VK_NULL_HANDLE) return false;

    return true;
}

bool hive::vk::VulkanRenderer::createCommandBuffer()
{
    create_command_buffer(device_, command_buffers_, MAX_FRAME_IN_FLIGHT);
    if(command_buffers_[0] == VK_NULL_HANDLE) return false;

    return  true;
}

bool hive::vk::VulkanRenderer::createSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(i32 i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device_.logical_device, &semaphoreInfo, nullptr, &image_available_semaphore_[i]) !=
            VK_SUCCESS ||
            vkCreateSemaphore(device_.logical_device, &semaphoreInfo, nullptr, &render_finished_semaphore_[i]) !=
            VK_SUCCESS ||
            vkCreateFence(device_.logical_device, &fenceInfo, nullptr, &in_flight_fence_[i]) != VK_SUCCESS)
        {
            LOG_ERROR("failed to create synchronization objects for a frame!");
            return false;
        }
    }

    return true;

}

void hive::vk::VulkanRenderer::recreateSwapChain()
{
    i32 width = 0, height = 0;
    do
    {
        window_.getFramebufferSize(width, height);
        window_.waitEvents();
    } while(width == 0 || height == 0);
    vkDeviceWaitIdle(device_.logical_device);

    destroy_swapchain(device_, swapchain_);
    createSwapChain(window_);
    createFramebuffers();
}

bool checkValidationLayerSupport(const std::vector<const char*> &validationLayers)
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName: validationLayers)
    {
        bool layerFound = false;

        for (const auto &layerProperties: availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

std::vector<const char *> getRequiredExtensions(const hive::Window& window, bool enable_validation)
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;

    std::vector<const char *> extensions;

    window.appendRequiredVulkanExtension(extensions);

    if (enable_validation)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void hive::vk::VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}
