#include <swarm/platform/diligent_swarm.h>
#include <swarm/platform/linux_swarm.h>
#include <swarm/precomp.h>
#include <swarm/swarm.h>

#include <EngineFactoryVk.h>

#define TERRA_NATIVE_LINUX
#include <terra/terra.h>
#include <terra/terra_native.h>

namespace swarm
{
    extern bool InitRenderContextCommon(RenderContext* renderContext);

    bool InitRenderContext(RenderContext* renderContext, terra::WindowContext* window)
    {
        terra::NativeWindow native = terra::GetNativeWindow(window);
        switch (native.m_type)
        {
            case terra::NativeWindowType::X11:
                return InitRenderContextX11(*renderContext, native.m_x11Display, native.m_x11Window,
                                            static_cast<uint32_t>(terra::GetWindowWidth(window)),
                                            static_cast<uint32_t>(terra::GetWindowHeight(window)));
            case terra::NativeWindowType::WAYLAND:
                return InitRenderContextWayland(*renderContext, native.m_wlDisplay, native.m_wlSurface,
                                                static_cast<uint32_t>(terra::GetWindowWidth(window)),
                                                static_cast<uint32_t>(terra::GetWindowHeight(window)));
        }
        return false;
    }

    bool InitRenderContextWayland(RenderContext& renderContext, wl_display* display, wl_surface* surface,
                                  uint32_t width, uint32_t height)
    {
        if (!InitRenderContextCommon(&renderContext))
        {
            return false;
        }

        if (display && surface)
        {
            // Create swapchain
            auto* factory = Diligent::GetEngineFactoryVk();

            Diligent::SwapChainDesc desc;
            desc.Width = width;
            desc.Height = height;

            Diligent::LinuxNativeWindow nativeWindow;

            nativeWindow.pDisplay = display;
            nativeWindow.pWaylandSurface = surface;
            factory->CreateSwapChainVk(renderContext.m_device, renderContext.m_context, desc, nativeWindow,
                                       &renderContext.m_swapchain);
        }

        return true;
    }

    bool InitRenderContextX11(RenderContext& renderContext, Display* display, Window window, uint32_t width,
                              uint32_t height)
    {
        if (!InitRenderContextCommon(&renderContext))
        {
            return false;
        }

        if (display && window)
        {
            // Create swapchain
            auto* factory = Diligent::GetEngineFactoryVk();

            Diligent::SwapChainDesc desc;
            desc.Width = width;
            desc.Height = height;
            Diligent::LinuxNativeWindow nativeWindow;

            nativeWindow.pDisplay = display;
            nativeWindow.WindowId = window;
            factory->CreateSwapChainVk(renderContext.m_device, renderContext.m_context, desc, nativeWindow,
                                       &renderContext.m_swapchain);
        }

        return true;
    }
} // namespace swarm
