#include "rendering/RendererPlatform.h"

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "Renderer_Vulkan.h"
#include "vulkan_device.h"
#include "core/Logger.h"
#include <unordered_set>
#include <string>

namespace hive::vk
{
    // bool is_device_suitable(const VkPhysicalDevice &device, const VkSurfaceKHR surface_khr,
    //                       const PhysicalDeviceRequirements &requirements,
    //                       PhysicalDeviceFamilyQueueInfo &out_familyQueueInfo)
    // {
    //
    // }
    bool is_device_suitable(const VkPhysicalDevice &device, const VkSurfaceKHR &surface_khr, const PhysicalDeviceRequirements &requirements,
                          PhysicalDeviceFamilyQueueInfo &out_familyQueueInfo)
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

            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_khr, &surface_supported);
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
              (!requirements.presentation || (requirements.presentation && out_familyQueueInfo.present_family_index != -
                                              1)) &&
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

            std::unordered_set<std::string> requiredExtensions(requirements.extensions.begin(),
                                                               requirements.extensions.end());

            for (const auto &extension: availableExtensions)
            {
                requiredExtensions.erase(extension.extensionName);
            }

            if (!requiredExtensions.empty())
            {
                return false;
            }
        }

        return true;
    }

    void create_logical_device(const VkInstance &instance, VulkanDevice &device)
    {
        //TODO: Adapt the code to create multiple family queue in case the index are not shared for each supported type (present, transfer, graphics)
        //See kohi code for that

        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = device.graphics_queue_index_;
        queueCreateInfo.queueCount = 1;

        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;

        createInfo.pEnabledFeatures = &deviceFeatures;

        //TODO: is it the best place to define the required extensions for the program?
        const char* extension_name[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = extension_name;

        createInfo.enabledLayerCount = 0;

        const VkResult result = vkCreateDevice(device.physicalDevice_, &createInfo, nullptr, &device.device_);
        if (result != VK_SUCCESS) return;

        vkGetDeviceQueue(device.device_, device.graphics_queue_index_, 0, &device.graphicsQueue_);
    }

    void pick_physical_device(const PhysicalDeviceRequirements& requirements, VulkanDevice& vulkan_device, const VkSurfaceKHR &surface, const VkInstance& instance)
    {
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) return;

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto &device: devices)
        {
            PhysicalDeviceFamilyQueueInfo familyQueueInfo{};
            if (is_device_suitable(device, surface, requirements, familyQueueInfo))
            {
                vulkan_device.physicalDevice_ = device;
                vulkan_device.graphics_queue_index_ = familyQueueInfo.graphics_family_index;
                vulkan_device.present_queue_index_ = familyQueueInfo.present_family_index;
                vulkan_device.transfer_queue_index_ = familyQueueInfo.transfer_family_index;
                break;
            }
        }


    }
}

#endif
