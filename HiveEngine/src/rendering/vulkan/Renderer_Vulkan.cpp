
#include "rendering/RendererPlatform.h"

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <core/Window.h>
#include <unordered_set>
#include <string>
#include "vulkan/vulkan.h"
#include <vector>

#include "Renderer_Vulkan.h"
#include <core/Logger.h>


hive::vk::RendererVulkan::RendererVulkan(const Window &window)
{
    LOG_INFO("Initializing Vulkan renderer");
    if (!createInstance(window)) return;
    if (!createSurface(window)) return;
    if (!pickPhysicalDevice()) return;
    if (!createLogicalDevice()) return;
    if (!createSwapChain()) return;

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

hive::ShaderProgramHandle hive::vk::RendererVulkan::createProgram()
{
    return {0};
}

void hive::vk::RendererVulkan::destroyProgram(ShaderProgramHandle shader)
{
}

void hive::vk::RendererVulkan::useProgram(ShaderProgramHandle shader)
{
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
    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (deviceCount == 0) return false;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    PhysicalDeviceRequirements requirements{};
    requirements.graphics = true;
    requirements.presentation = true;
    requirements.discrete_gpu = false;

    requirements.extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    for (const auto& device : devices) {
        PhysicalDeviceFamilyQueueInfo familyQueueInfo{};
        if (isDeviceSuitable(device, requirements, familyQueueInfo)) {
            device_.physicalDevice_ = device;
            device_.graphics_queue_index_ = familyQueueInfo.graphics_family_index;
            device_.present_queue_index_ = familyQueueInfo.present_family_index;
            device_.transfer_queue_index_ = familyQueueInfo.transfer_family_index;
            break;
        }
    }

    if (device_.physicalDevice_ == VK_NULL_HANDLE)
    {
        LOG_ERROR("Failed to find a suitable physical device");
        return false;
    }

    return true;
}

bool hive::vk::RendererVulkan::createLogicalDevice()
{
    //TODO: Adapt the code to create multiple family queue in case the index are not shared for each supported type (present, transfer, graphics)
    //See kohi code for that

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = device_.graphics_queue_index_;
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = 0;
    createInfo.enabledLayerCount = 0;

    const VkResult result = vkCreateDevice(device_.physicalDevice_, &createInfo, nullptr, &device_.device_);
    if (result != VK_SUCCESS) return false;

    vkGetDeviceQueue(device_.device_, device_.graphics_queue_index_, 0, &device_.graphicsQueue_);

    return true;
}

bool hive::vk::RendererVulkan::createSwapChain()
{

    return true;
}

bool hive::vk::RendererVulkan::isDeviceSuitable(const VkPhysicalDevice &device,
                                                const PhysicalDeviceRequirements &requirements,
                                                PhysicalDeviceFamilyQueueInfo &out_familyQueueInfo) const
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);

    if (requirements.discrete_gpu && properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        LOG_INFO("Device is not a discrete GPU. Skipping");
        return false;
    }

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    VkQueueFamilyProperties queue_families[32];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);



    VkBool32 surface_supported = VK_FALSE;
    for (u32 i = 0; i < queue_family_count; i++)
    {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            out_familyQueueInfo.graphics_family_index = i;
        }

        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            out_familyQueueInfo.compute_family_index = i;
        }

        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            out_familyQueueInfo.transfer_family_index = i;
        }

        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &surface_supported);
        if (surface_supported == VK_TRUE)
        {
            out_familyQueueInfo.present_family_index = i;
        }

    }

    LOG_INFO("Information about the graphic device: %s (Discrete GPU: %s)", properties.deviceName,
             properties.deviceType & VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "TRUE" : "FALSE");
    LOG_INFO("Graphics support: %d, Compute support: %d, Present support: %d, Transfer support: %d",
             out_familyQueueInfo.graphics_family_index != -1, out_familyQueueInfo.compute_family_index != -1,
             out_familyQueueInfo.present_family_index != -1, out_familyQueueInfo.transfer_family_index != -1);

    if (!((!requirements.graphics || (requirements.graphics && out_familyQueueInfo.graphics_family_index != -1)) &&
        (!requirements.discrete_gpu || (requirements.discrete_gpu && surface_supported)) &&
        (!requirements.compute || (requirements.compute && out_familyQueueInfo.compute_family_index != -1)) &&
        (!requirements.presentation || (requirements.presentation && out_familyQueueInfo.present_family_index != -1)) &&
        (!requirements.transfer || (requirements.transfer && out_familyQueueInfo.transfer_family_index != -1))))
    {
        return false;
    }

    if (requirements.extensions.size() != 0)
    {
        u32 extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::unordered_set<std::string> requiredExtensions(requirements.extensions.begin(), requirements.extensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        if (!requiredExtensions.empty())
        {
            return false;
        }
    }

    return true;
}

#endif
