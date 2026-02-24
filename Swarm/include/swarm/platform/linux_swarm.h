#pragma once
#include <swarm/swarm.h>

#include <wayland-client.h>
#include <X11/Xlib.h>

#undef Bool
#undef True
#undef False

namespace swarm
{
    //display and surface can be null for offscreen rendering
    bool InitRenderContextWayland(RenderContext &renderContext, wl_display *display, wl_surface *surface,
                                   uint32_t width, uint32_t height);

    //display and window can be null for offscreen rendering
    bool InitRenderContextX11(RenderContext &renderContext, Display *display, Window window,
                              uint32_t width, uint32_t height);
}
