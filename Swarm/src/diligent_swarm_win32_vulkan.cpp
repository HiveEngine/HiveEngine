#include <swarm/precomp.h>
#include <swarm/swarm.h>
#include <swarm/platform/win32_swarm.h>

#include <EngineFactoryVk.h>

#include "swarm/platform/diligent_swarm.h"

namespace swarm
{
    extern bool InitRenderContextCommon(RenderContext *renderContext);

    bool InitRenderContextWin32(RenderContext *renderContext, HINSTANCE instance, HWND window)
    {
        if (!InitRenderContextCommon(renderContext))
        {
            return false;
        }

        if (window)
        {
            auto* factory = Diligent::GetEngineFactoryVk();
            Diligent::SwapChainDesc swapChainDesc;
            swapChainDesc.Width = 600;
            swapChainDesc.Height = 500;

            Diligent::Win32NativeWindow nativeWindow;
            nativeWindow.hWnd = window;
            factory->CreateSwapChainVk(renderContext->device_, renderContext->context_, swapChainDesc, nativeWindow, &renderContext->swapchain_);
        }

        return true;
    }
}
