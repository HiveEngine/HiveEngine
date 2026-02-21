#include <swarm/precomp.h>
#include <swarm/swarm.h>
#include <swarm/platform/linux_swarm.h>
#include <swarm/platform/diligent_swarm.h>

#include <EngineFactoryVk.h>
namespace swarm
{
    extern bool InitRenderContextCommon(RenderContext *renderContext);

    bool InitRenderContextWayland(RenderContext *renderContext, wl_display *display, wl_surface *surface)
    {
        if (!InitRenderContextCommon(renderContext))
        {
            return false;
        }

        if (display && surface)
        {
            //Create swapchain
            auto* factory = Diligent::GetEngineFactoryVk();

            //TODO get this info from function parameter
            Diligent::SwapChainDesc desc;
            desc.Width = 600;
            desc.Height = 500;


            Diligent::LinuxNativeWindow nativeWindow;

            nativeWindow.pDisplay = display;
            nativeWindow.pWaylandSurface = surface;
            factory->CreateSwapChainVk(renderContext->device_, renderContext->context_, desc, nativeWindow, &renderContext->swapchain_);
        }

        return true;
    }

    bool InitRenderContextX11(RenderContext *renderContext, Display *display, Window window)
    {
        if (!InitRenderContextCommon(renderContext))
        {
            return false;
        }

        if (display && window)
        {
            //Create swapchain
            auto* factory = Diligent::GetEngineFactoryVk();

            Diligent::SwapChainDesc desc;
            Diligent::LinuxNativeWindow nativeWindow;

            nativeWindow.pDisplay = display;
            nativeWindow.WindowId = window;
            factory->CreateSwapChainVk(renderContext->device_, renderContext->context_, desc, nativeWindow, &renderContext->swapchain_);
        }

        return true;
    }
}