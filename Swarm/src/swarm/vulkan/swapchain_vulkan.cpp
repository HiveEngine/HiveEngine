#include <swarm/precomp.h>

#include <swarm/swapchain.h>
#include <swarm/surface.h>
#include <swarm/device.h>

#include <iostream>

namespace swarm
{

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(VkPhysicalDevice physDevice, VkSurfaceKHR surface)
    {
        unsigned int surfaceFormatCount {0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &surfaceFormatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &surfaceFormatCount, surfaceFormats.data());

        //Make sure the ideal surface format is present
        for (const auto& surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return surfaceFormat;
            }
        }

        //TODO Log some warning i guess
        return surfaceFormats[0];
    }

    Swapchain::Swapchain(Device &device, SwapchainDescription description) : m_Device{device}
    {
        vkb::SwapchainBuilder builder{device, description.surface};
        const auto swapchainRes = builder.build();

        if (!swapchainRes.has_value())
        {
            const vkb::Error error = swapchainRes.full_error();
            std::cout << error.detailed_failure_reasons[0] << std::endl;

            return;
        }

        m_Swapchain = swapchainRes.value();

        const auto swapchainImageViewRes = m_Swapchain.get_image_views();
        if (!swapchainImageViewRes.has_value())
            return;

        m_SwapchainImageViews = swapchainImageViewRes.value();
    }

    Swapchain::~Swapchain()
    {
        for (const auto &imageView: m_SwapchainImageViews)
        {
            vkDestroyImageView(m_Device, imageView, nullptr);
        }
        vkb::destroy_swapchain(m_Swapchain);
    }
}
