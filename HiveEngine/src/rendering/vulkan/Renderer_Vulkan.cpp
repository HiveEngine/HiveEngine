
#include "vulkan_image.h"
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

    //Default shader
    ShaderProgramHandle shader = createProgram("shaders/vert.spv", "shaders/frag.spv");
    if (shader.id == -1) return;

    is_ready_ = true;
}

hive::vk::RendererVulkan::~RendererVulkan()
{
    LOG_INFO("Shutting down Vulkan renderer");
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
}

void hive::vk::RendererVulkan::endDrawing()
{
}

void hive::vk::RendererVulkan::frame()
{
}

hive::ShaderProgramHandle hive::vk::RendererVulkan::createProgram(const char *vertex_path, const char *fragment_path)
{
    return create_shader_program(vertex_path, fragment_path, device_, swapchain_);
}

void hive::vk::RendererVulkan::destroyProgram(ShaderProgramHandle shader)
{
    destroy_program(shader);
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


#endif
