#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <vulkan/vulkan.h>
#include "vulkan_utils.h"
#include "vulkan_types.h"
#include <vector>
#include <cstring>
#include <string>
#include <set>
#include <core/Logger.h>


bool hive::vk::check_validation_layer_support(const std::vector<const char *> &validation_layers)
{
    u32 layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (i32 i = 0; i < validation_layers.size(); i++)
    {
        const char *layerName = validation_layers[i];
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

bool hive::vk::check_extension_support(VkPhysicalDevice physical_device,
    const std::vector<const char *> &required_extensions)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(required_extensions.begin(), required_extensions.end());

    for (const auto &extension: availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

hive::vk::SwapChainSupportDetails hive::vk::vk_querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

hive::vk::QueueFamilyIndices hive::vk::vK_findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface_khr)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily: queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_khr, &presentSupport);

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

VkCommandBuffer hive::vk::begin_single_command(const VulkanDevice& device)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.graphics_command_pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.logical_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void hive::vk::end_single_command(const VulkanDevice& device, VkCommandBuffer command_buffer)
{
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;

    vkQueueSubmit(device.graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(device.graphics_queue);

    vkFreeCommandBuffers(device.logical_device, device.graphics_command_pool, 1, &command_buffer);
}

VkFormat hive::vk::find_supported_format(const VulkanDevice &device, const std::vector<VkFormat> &candidates,
    VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for(VkFormat format : candidates)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device.physical_device, format, &properties);

        if(tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) return format;
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features) return format;
    }

    LOG_ERROR("Vulkan: failed to find supported format");
    return {};
}

VkFormat hive::vk::find_depth_format(const VulkanDevice &device)
{
    return find_supported_format(
        device,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool hive::vk::findMemoryType(const hive::vk::VulkanDevice &device, u32 typeFilter, VkMemoryPropertyFlags properties, u32 &out_index)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(device.physical_device, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            out_index = i;
            return true;
        }
    }

    return false;
}


#endif
