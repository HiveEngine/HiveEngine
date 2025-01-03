#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <hvpch.h>
#include "vulkan_swapchain.h"
#include "vulkan_device.h"
#include <core/Logger.h>

#include <set>
#include <string>
namespace hive::vk
{

    void pick_physical_device(const VkInstance& instance, const VkSurfaceKHR& surface, Device &device, DeviceConfig& config);
    void create_logical_device(const VkInstance &instance, const VkSurfaceKHR &surface_khr, Device &device, DeviceConfig& config);

    bool is_device_suitable(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const DeviceConfig &config);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*> &required_extensions);



    void create_device(const VkInstance &instance, const VkSurfaceKHR &surface_khr, Device &device)
    {
        DeviceConfig config;
        config.enable_validation_layers = true;

        pick_physical_device(instance, surface_khr, device, config);


        if(device.physical_device == VK_NULL_HANDLE) return;

        create_logical_device(instance, surface_khr, device, config);

    }

    void destroy_device(const Device &device)
    {
        vkDestroyDevice(device.logical_device, nullptr);
    }


    void pick_physical_device(const VkInstance& instance, const VkSurfaceKHR& surface, Device &device, DeviceConfig& config)
    {
        u32 device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

        if(device_count == 0)
        {
            LOG_ERROR("Failed to find GPUs with Vulkan support");
            return;
        }


        VkPhysicalDevice devices[16];
        vkEnumeratePhysicalDevices(instance, &device_count, devices);

        for(u32 i = 0; i < device_count; i++)
        {
            if(is_device_suitable(devices[i], surface, config))
            {
                device.physical_device = devices[i];
                return;
            }
        }

        LOG_ERROR("Failed to find a suitable physical device");

    }

    void create_logical_device(const VkInstance &instance, const VkSurfaceKHR &surface_khr, Device &device, DeviceConfig& config)
    {
        VKQueueFamilyIndices indices = findQueueFamilies(device.physical_device, surface_khr);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<u32> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily: uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(config.required_extensions.size());
        createInfo.ppEnabledExtensionNames = config.required_extensions.data();

        if (config.enable_validation_layers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(config.validation_layers.size());
            createInfo.ppEnabledLayerNames = config.validation_layers.data();
        } else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(device.physical_device, &createInfo, nullptr, &device.logical_device) != VK_SUCCESS)
        {
            LOG_ERROR("failed to create logical device!");
        }

        vkGetDeviceQueue(device.logical_device, indices.graphicsFamily.value(), 0, &device.graphics_queue);
        vkGetDeviceQueue(device.logical_device, indices.presentFamily.value(), 0, &device.present_queue);

    }

    bool is_device_suitable(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const DeviceConfig &config)
    {
        VKQueueFamilyIndices indices = findQueueFamilies(physical_device, surface);


        //TODO: check if we should put the required extension to another location

        bool extensionsSupported = checkDeviceExtensionSupport(physical_device, config.required_extensions);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            VKSwapChainSupportDetails swapChainSupport = querySwapChainSupport(physical_device, surface);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*> &required_extensions)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(required_extensions.begin(), required_extensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    VKQueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface_khr)
    {
        VKQueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_khr, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }


}
#endif
