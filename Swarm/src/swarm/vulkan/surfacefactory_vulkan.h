#pragma once

class wl_display;
class wl_surface;
class _XDisplay;

namespace swarm::vk
{
    class SurfaceFactory
    {
    public:
        static VkSurfaceKHR CreateWaylandSurface(VkInstance instance, wl_display* display, wl_surface* surface);
        static VkSurfaceKHR CreateX11Surface(VkInstance instance, unsigned long windowId, _XDisplay* display);
        static VkSurfaceKHR CreateWin32Surface(VkInstance instance);
    };
}