#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_swapchain.h"
#include <core/Window.h>
#include <limits>
#include <algorithm>

namespace hive::vk
{
    void create_swapchain(const VkInstance &instance, const VkSurfaceKHR&surface, const VulkanDevice &vulkan_device,
                                    VulkanSwapchain &swapchain, const Window& window)
    {
        //Get some information about the available formats
        VkSurfaceCapabilitiesKHR capabilities_khr{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_device.physicalDevice_, surface, &capabilities_khr);

        u32 format_count;
        VkSurfaceFormatKHR formats[32];
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan_device.physicalDevice_, surface, &format_count, nullptr);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan_device.physicalDevice_, surface, &format_count, formats);

        u32 present_mode_count;
        VkPresentModeKHR present_modes[32];
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan_device.physicalDevice_, surface, &present_mode_count, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan_device.physicalDevice_, surface, &present_mode_count, present_modes);


        //Pick surface format
        u32 picked_format = 0;
        for(i32 i = 0; i < format_count; i++)
        {
            auto format = formats[i];
            if(formats->format == VK_FORMAT_B8G8R8_SRGB && formats->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                picked_format = i;
                break;
            }
        }


        //Pick presentation mode
        VkPresentModeKHR picked_present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for(i32 i = 0; i < present_mode_count; i++)
        {
            auto present_mode = present_modes[i];
            if(present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                picked_present_mode = present_mode;
                break;
            }
        }

        //Pick extent
        VkExtent2D extent;
        if(capabilities_khr.currentExtent.width != std::numeric_limits<u32>::max())
        {
            extent = capabilities_khr.currentExtent;
        }
        else
        {
            i32 width, height;
            window.getFramebufferSize(width, height);
            extent.width = std::clamp(static_cast<u32>(width), capabilities_khr.minImageExtent.width, capabilities_khr.maxImageExtent.width);
            extent.height = std::clamp(static_cast<u32>(height), capabilities_khr.minImageExtent.height, capabilities_khr.maxImageExtent.height);
        }

        u32 image_count = capabilities_khr.minImageCount + 1;
        if(capabilities_khr.maxImageCount > 0 && image_count > capabilities_khr.maxImageCount)
        {
            image_count = capabilities_khr.maxImageCount;
        }

        VkSwapchainCreateInfoKHR create_info{};

        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = formats[picked_format].format;
        create_info.imageColorSpace = formats[picked_format].colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        u32 queue_family_indices[] = {static_cast<u32>(vulkan_device.graphics_queue_index_), static_cast<u32>(vulkan_device.present_queue_index_)};
        if(vulkan_device.graphics_queue_index_ != vulkan_device.present_queue_index_)
        {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; //Image can be used with multiple queue family without explicit ownership transfer
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        }
        else
        {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; //Image is owner by one queue family at a time. Need explicit ownership transfer
            create_info.queueFamilyIndexCount = 0;
            create_info.pQueueFamilyIndices = nullptr;
        }

        create_info.preTransform = capabilities_khr.currentTransform;

        create_info.presentMode = present_modes[picked_present_mode];
        create_info.clipped = VK_TRUE; //We don't care about the pixel value that are obscured by another one

        VkResult result = vkCreateSwapchainKHR(vulkan_device.device_, &create_info, nullptr, &swapchain.swapchain_khr);
        if(result != VK_SUCCESS)
        {
            return;
        }

        vkGetSwapchainImagesKHR(vulkan_device.device_, swapchain.swapchain_khr, &swapchain.image_count, nullptr);
        vkGetSwapchainImagesKHR(vulkan_device.device_, swapchain.swapchain_khr, &swapchain.image_count, swapchain.images);

        swapchain.format = formats[picked_format].format;
        swapchain.extent = extent;

    }

}

#endif

