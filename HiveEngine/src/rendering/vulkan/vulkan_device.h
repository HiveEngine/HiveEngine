#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>
namespace hive::vk
{
    struct DeviceConfig
    {
        bool enable_validation_layers;

        std::vector<const char*> required_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        std::vector<const char *> validation_layers = {
            "VK_LAYER_KHRONOS_validation"
        };


    };

    struct Device
    {

        VkPhysicalDevice physical_device;
        VkDevice logical_device;

        VkQueue graphics_queue;
        VkQueue present_queue;

        VkCommandPool graphics_command_pool;

    };

    struct VKQueueFamilyIndices
    {
        std::optional<u32> graphicsFamily;
        std::optional<u32> presentFamily;

        [[nodiscard]] bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };



    void create_device(const VkInstance &instance, const VkSurfaceKHR &surface_khr, Device &device);
    void destroy_device(const Device &device);

    VKQueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface_khr);
}