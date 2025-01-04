#pragma once
#include <vulkan/vulkan.h>

namespace hive
{
    class Window;
}

namespace hive::vk
{
    struct VulkanDevice;
    struct VulkanSwapchain;

    bool create_surface(const VkInstance& instance, const Window& window, VkSurfaceKHR& surface_khr);

    bool create_swapchain(const VulkanDevice &device, const VkSurfaceKHR &surface_khr, const Window &window,
                          VulkanSwapchain &out_swapchain);

    void destroy_swapchain(const VulkanDevice &device, VulkanSwapchain& swapchain);
    void destroy_surface(const VkInstance& instance, VkSurfaceKHR& surface_khr);
}
