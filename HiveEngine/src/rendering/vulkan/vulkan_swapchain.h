#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace hive
{
    class Window;
}
namespace hive::vk
{

    struct Device;

    struct VKSwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct Swapchain
    {
        VkSwapchainKHR swapchain_khr;
        std::vector<VkImage> images;
        VkFormat image_format;
        VkExtent2D extent_2d;

        std::vector<VkImageView> image_views;
    };

    void create_swapchain(const Device &device,
                          const Window &window,
                          const VkSurfaceKHR &surface_khr,
                          Swapchain &swapchain);

    void destroy_swapchain(const Device& device, const Swapchain& swapchain);

    VKSwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);


}