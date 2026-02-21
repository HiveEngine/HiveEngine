#include <swarm/precomp.h>
#include <swarm/swarm.h>
#include <swarm/swarm_log.h>
#include <swarm/platform/diligent_swarm.h>

#include <hive/core/log.h>

#include <EngineFactoryVk.h>
namespace swarm
{
    const hive::LogCategory LogDiligent{"Diligent", &LogSwarm};

    void DiligentToHiveMessageCallback(Diligent::DEBUG_MESSAGE_SEVERITY severity,
                                       const Diligent::Char *message,
                                       const Diligent::Char *function,
                                       const Diligent::Char *file,
                                       int line)
    {
        switch (severity)
        {
            case Diligent::DEBUG_MESSAGE_SEVERITY_INFO:
                hive::LogInfo(LogDiligent, message);
                break;
            case Diligent::DEBUG_MESSAGE_SEVERITY_WARNING:
                hive::LogWarning(LogDiligent, message);
                break;
            case Diligent::DEBUG_MESSAGE_SEVERITY_ERROR:
                hive::LogError(LogDiligent, message);
                break;
            case Diligent::DEBUG_MESSAGE_SEVERITY_FATAL_ERROR:
                hive::LogError(LogDiligent, message);
                break;
        }
    }

    bool InitSystem()
    {
        Diligent::SetDebugMessageCallback(&DiligentToHiveMessageCallback);
        return true;
    }

    void ShutdownSystem()
    {
    }

    void ShutdownRenderContext(RenderContext *renderContext)
    {
        if (renderContext->swapchain_)
        {
            renderContext->swapchain_->Release();
        }

        if (renderContext->context_)
        {
            renderContext->context_->Release();
        }

        if (renderContext->device_)
        {
            renderContext->device_->Release();
        }
    }


    void Render(RenderContext *renderContext)
    {
        auto* pRTV = renderContext->swapchain_->GetCurrentBackBufferRTV();
        auto* pDSV = renderContext->swapchain_->GetDepthBufferDSV();

        renderContext->context_->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

        renderContext->context_->ClearRenderTarget(pRTV, clearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        renderContext->swapchain_->Present();
    }
}
