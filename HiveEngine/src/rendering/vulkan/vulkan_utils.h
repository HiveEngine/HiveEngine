#pragma once
#include <hvpch.h>
#include <vector>
#include <optional>

#include <vulkan/vulkan.h>
namespace hive::vk
{
    struct VulkanDevice;

    bool check_validation_layer_support(const std::vector<const char*> &validation_layers);
    bool check_extension_support(VkPhysicalDevice physical_device, const std::vector<const char*> &required_extensions);

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats{};
        std::vector<VkPresentModeKHR> presentModes{};
    };

    SwapChainSupportDetails vk_querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    struct QueueFamilyIndices
    {
        std::optional<u32> graphicsFamily;
        std::optional<u32> presentFamily;

        [[nodiscard]] bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };


    QueueFamilyIndices vK_findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface_khr);

    bool findMemoryType(const VulkanDevice &device, u32 typeFilter, VkMemoryPropertyFlags properties, u32 &out_index);

    VkCommandBuffer begin_single_command(const VulkanDevice &device);
    void end_single_command(const VulkanDevice& device, VkCommandBuffer command_buffer);

}
