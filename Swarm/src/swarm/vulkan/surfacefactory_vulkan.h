#pragma once

class wl_display;
class wl_surface;
class _XDisplay;

struct HINSTANCE__;
struct HWND__;

namespace swarm::vk
{
    class SurfaceFactory
    {
    public:
#if defined(__linux__)
        static VkSurfaceKHR CreateWaylandSurface(VkInstance instance, wl_display* display, wl_surface* surface);
        static VkSurfaceKHR CreateX11Surface(VkInstance instance, unsigned long windowId, _XDisplay* display);
#endif

#if defined(_WIN64)
        static VkSurfaceKHR CreateWin32Surface(VkInstance instance, HINSTANCE__* hinstance, HWND__* hwnd);
#endif
    };
}