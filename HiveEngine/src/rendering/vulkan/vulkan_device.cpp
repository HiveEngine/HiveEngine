#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_device.h"
#include "vulkan_config.h"
#include "vulkan_utils.h"
#include "vulkan_types.h"
#include <core/Logger.h>

#include <vector>
#include <set>
#include <string>

namespace hive::vk
{
    void pick_physical_device(const VkInstance &instance,
                              const VkSurfaceKHR &surface_khr,
                              VkPhysicalDevice &out_physical_device);

    void create_logical_device(const VkSurfaceKHR &surface_khr, VulkanDevice &device);

    bool is_device_suitable(VkPhysicalDevice physical_device, VkSurfaceKHR surface_khr);


    bool create_device(const VkInstance &instance, const VkSurfaceKHR &surface_khr, VulkanDevice &device)
    {
        pick_physical_device(instance, surface_khr, device.physical_device);

        if (device.physical_device == VK_NULL_HANDLE) return false;

        create_logical_device(surface_khr, device);

        if (device.logical_device == VK_NULL_HANDLE) return false;

        return true;
    }

    void destroy_device(VulkanDevice &device)
    {
        vkDestroyDevice(device.logical_device, nullptr);
        device.logical_device = VK_NULL_HANDLE;
    }


    void pick_physical_device(const VkInstance &instance, const VkSurfaceKHR &surface_khr,
                              VkPhysicalDevice &out_physical_device)
    {
        u32 device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

        if (device_count == 0)
        {
            LOG_ERROR("Failed to find GPUs with Vulkan support");
            return;
        }


        VkPhysicalDevice devices[16];
        vkEnumeratePhysicalDevices(instance, &device_count, devices);

        for (u32 i = 0; i < device_count; i++)
        {
            if (is_device_suitable(devices[i], surface_khr))
            {
                out_physical_device = devices[i];
                return;
            }
        }

        LOG_ERROR("Failed to find a suitable physical device");
    }

    void create_logical_device(const VkSurfaceKHR &surface_khr, VulkanDevice &device)
    {
        QueueFamilyIndices indices = vK_findQueueFamilies(device.physical_device, surface_khr);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<u32> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        f32 queuePriority = 1.0f;
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
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(config::required_extensions.size());
        createInfo.ppEnabledExtensionNames = config::required_extensions.data();

        if (config::enable_validation)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(config::validation_layers.size());
            createInfo.ppEnabledLayerNames = config::validation_layers.data();
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


    bool is_device_suitable(VkPhysicalDevice physical_device, VkSurfaceKHR surface_khr)
    {
        QueueFamilyIndices indices = vK_findQueueFamilies(physical_device, surface_khr);


        bool extensionsSupported = check_extension_support(physical_device,
                                                           config::required_extensions);

        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            SwapChainSupportDetails swapChainSupport = vk_querySwapChainSupport(
                physical_device, surface_khr);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physical_device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }
}

#endif
