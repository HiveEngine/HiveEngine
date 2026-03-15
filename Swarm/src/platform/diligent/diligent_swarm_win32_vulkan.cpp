#include <swarm/platform/win32_swarm.h>
#include <swarm/precomp.h>
#include <swarm/swarm.h>

#include <EngineFactoryVk.h>

#include "swarm/platform/diligent_swarm.h"

#define TERRA_NATIVE_WIN32
#include <terra/terra.h>
#include <terra/terra_native.h>

namespace swarm
{
    extern bool InitRenderContextCommon(RenderContext* renderContext);

    bool InitRenderContext(RenderContext* renderContext, terra::WindowContext* window)
    {
        terra::NativeWindow native = terra::GetNativeWindow(window);
        return InitRenderContextWin32(renderContext, native.m_instance, native.m_window,
                                      static_cast<uint32_t>(terra::GetWindowWidth(window)),
                                      static_cast<uint32_t>(terra::GetWindowHeight(window)));
    }

    bool InitRenderContextWin32(RenderContext* renderContext, HINSTANCE instance, HWND window, uint32_t width,
                                uint32_t height)
    {
        if (!InitRenderContextCommon(renderContext))
        {
            return false;
        }

        if (window)
        {
            auto* factory = Diligent::GetEngineFactoryVk();
            Diligent::SwapChainDesc swapChainDesc;
            swapChainDesc.Width = width;
            swapChainDesc.Height = height;

            Diligent::Win32NativeWindow nativeWindow;
            nativeWindow.hWnd = window;
            factory->CreateSwapChainVk(renderContext->m_device, renderContext->m_context, swapChainDesc, nativeWindow,
                                       &renderContext->m_swapchain);
        }

        return true;
    }
} // namespace swarm
