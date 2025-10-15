#include "surfacefactory_vulkan.h"

#if defined(__linux__)
#include <vulkan/vulkan_wayland.h>
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
namespace swarm::vk
{
    VkSurfaceKHR SurfaceFactory::CreateWaylandSurface(VkInstance instance, wl_display* display, wl_surface* surface)
    {
        VkWaylandSurfaceCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.display = display;

        VkSurfaceKHR surfaceHandle{VK_NULL_HANDLE};
        vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &surfaceHandle);

        return surfaceHandle;
    }

    VkSurfaceKHR SurfaceFactory::CreateX11Surface(VkInstance instance, unsigned long windowId, _XDisplay* display)
    {
        VkXlibSurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        createInfo.dpy = display;
        createInfo.window = windowId;

        VkSurfaceKHR surface{VK_NULL_HANDLE};

        vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface);

        return surface;
    }

    VkSurfaceKHR SurfaceFactory::CreateWin32Surface(VkInstance instance)
    {
        return VK_NULL_HANDLE;
    }
}
#endif
