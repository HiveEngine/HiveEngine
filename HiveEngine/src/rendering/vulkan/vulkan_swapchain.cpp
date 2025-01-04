#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_swapchain.h"
#include "vulkan_utils.h"
#include "vulkan_types.h"

#include <core/Logger.h>
#include <core/Window.h>

#include <algorithm>
#include <limits>

namespace hive::vk
{
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> & vector);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> & vector);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR & capabilities, const Window & window);
    bool create_image_view(const VulkanDevice& device, VulkanSwapchain& out_swapchain);

    bool create_surface(const VkInstance &instance, const Window &window, VkSurfaceKHR &surface_khr)
    {
        window.createVulkanSurface(instance, &surface_khr);

        if (surface_khr == VK_NULL_HANDLE)
        {
            LOG_ERROR("Vulkan: failed to create window surface");
        }
        return true;
    }


    bool create_swapchain(const VulkanDevice &device, const VkSurfaceKHR &surface_khr, const Window& window, VulkanSwapchain &out_swapchain)
    {
        SwapChainSupportDetails swap_chain_support_details = vk_querySwapChainSupport(device.physical_device, surface_khr);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swap_chain_support_details.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swap_chain_support_details.presentModes);
        VkExtent2D extent = chooseSwapExtent(swap_chain_support_details.capabilities, window);

        uint32_t imageCount = swap_chain_support_details.capabilities.minImageCount + 1;
        if (swap_chain_support_details.capabilities.maxImageCount > 0 && imageCount > swap_chain_support_details.capabilities.maxImageCount)
        {
            imageCount = swap_chain_support_details.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface_khr;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = vK_findQueueFamilies(device.physical_device, surface_khr);

        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swap_chain_support_details.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device.logical_device, &createInfo, nullptr, &out_swapchain.vk_swapchain) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create swap chain!");
            return false;
        }

        vkGetSwapchainImagesKHR(device.logical_device, out_swapchain.vk_swapchain, &imageCount, nullptr);
        out_swapchain.images.resize(imageCount);
        vkGetSwapchainImagesKHR(device.logical_device, out_swapchain.vk_swapchain, &imageCount, out_swapchain.images.data());

        out_swapchain.image_format = surfaceFormat.format;
        out_swapchain.extent_2d = extent;

        return create_image_view(device, out_swapchain);
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
    {
        for (const auto &availableFormat: availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
    {
        for (const auto &availablePresentMode: availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, const Window &window)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        } else
        {
            int width, height;
            window.getFramebufferSize(width, height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                            capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                             capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    bool create_image_view(const VulkanDevice &device, VulkanSwapchain &out_swapchain)
    {
        out_swapchain.image_views.resize(out_swapchain.images.size());

        for (size_t i = 0; i < out_swapchain.images.size(); i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = out_swapchain.images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = out_swapchain.image_format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device.logical_device, &createInfo, nullptr, &out_swapchain.image_views[i]) !=
                VK_SUCCESS)
            {
                LOG_ERROR("Vulkan: failed to create image views!");
                return false;
            }
        }

        return true;
    }
}
#endif


