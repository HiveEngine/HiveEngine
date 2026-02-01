#include <swarm/precomp.h>
#include <swarm/swarm_module.h>

//Diligent
#include <EngineFactoryVk.h>

//GLFW
#include <GLFW/glfw3.h>

//STD
#include <vector>
#include <iostream>

namespace swarm
{
    Diligent::IRenderDevice* g_RenderDevice = nullptr;
    Diligent::IDeviceContext* g_DeviceContext = nullptr;
    Diligent::ISwapChain* g_SwapChain = nullptr;


    void InitVk(terra::Window &window)
    {
        auto *factory = Diligent::LoadAndGetEngineFactoryVk();
        Diligent::EngineVkCreateInfo engineInfo = {};


        // Get required Vulkan instance extensions from GLFW
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);


        // Set instance extensions
        std::vector<const char*> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        engineInfo.ppInstanceExtensionNames = instanceExtensions.data();
        engineInfo.InstanceExtensionCount = static_cast<unsigned int>(instanceExtensions.size());

        factory->CreateDeviceAndContextsVk(engineInfo, &g_RenderDevice, &g_DeviceContext);

        Diligent::LinuxNativeWindow LinuxWindow;
        const auto windowHandle = window.GetNativeHandle();
        LinuxWindow.pDisplay = windowHandle.displayHandle;
        LinuxWindow.pWaylandSurface = windowHandle.windowHandle;

        Diligent::SwapChainDesc SCDesc;
        SCDesc.ColorBufferFormat = Diligent::TEX_FORMAT_BGRA8_UNORM_SRGB;  // Vulkan commonly uses BGRA
        SCDesc.DepthBufferFormat = Diligent::TEX_FORMAT_D24_UNORM_S8_UINT;
        SCDesc.Usage = Diligent::SWAP_CHAIN_USAGE_RENDER_TARGET;

        // Get window size
        // int width, height;
        // glfwGetFramebufferSize(., &width, &height);
        SCDesc.Width = 920;
        SCDesc.Height = 720;

        factory->CreateSwapChainVk(g_RenderDevice, g_DeviceContext, SCDesc, LinuxWindow, &g_SwapChain);

        if (!g_SwapChain)
        {
            std::cerr << "Failed to create Vulkan swap chain" << std::endl;
        }

        std::cout << "Vulkan backend initialized successfully!" << std::endl;
        // std::cout << "Device: " << g_pDevice->GetDeviceInfo(). << std::endl;
    }


    void Shutdown()
    {
        // Flush any pending commands before cleanup
        if (g_DeviceContext)
        {
            g_DeviceContext->Flush();
            g_DeviceContext->FinishFrame();
        }

        // Release in reverse order
        // g_pSwapChain.Release();
        // g_pImmediateContext.Release();
        // g_pDevice.Release();
    }

    void Render()
    {

        if (!g_SwapChain || !g_DeviceContext)
            return;

        // Get the back buffer from the swap chain
        auto* pRTV = g_SwapChain->GetCurrentBackBufferRTV();
        auto* pDSV = g_SwapChain->GetDepthBufferDSV();

        // Set render targets
        g_DeviceContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Clear the back buffer to red
        const float ClearColor[] = {1.0f, 0.0f, 0.0f, 1.0f}; // Red color
        g_DeviceContext->ClearRenderTarget(pRTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (pDSV)
        {
            g_DeviceContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        // Present the frame
        g_SwapChain->Present();
    }

}
