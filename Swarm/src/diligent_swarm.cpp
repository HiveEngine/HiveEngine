#include <hive/core/log.h>

#include <swarm/platform/diligent_swarm.h>
#include <swarm/precomp.h>
#include <swarm/swarm.h>
#include <swarm/swarm_log.h>

#include <EngineFactoryVk.h>
#include <RefCntAutoPtr.hpp>
#include <Shader.h>
namespace swarm
{
    const hive::LogCategory LOG_DILIGENT{"Diligent", &LOG_SWARM};

    static void DiligentToHiveMessageCallback(Diligent::DEBUG_MESSAGE_SEVERITY severity, const Diligent::Char* message,
                                              const Diligent::Char* function, const Diligent::Char* file, int line) {
        switch (severity)
        {
            case Diligent::DEBUG_MESSAGE_SEVERITY_INFO:
                hive::LogInfo(LOG_DILIGENT, message);
                break;
            case Diligent::DEBUG_MESSAGE_SEVERITY_WARNING:
                hive::LogWarning(LOG_DILIGENT, message);
                break;
            case Diligent::DEBUG_MESSAGE_SEVERITY_ERROR:
            case Diligent::DEBUG_MESSAGE_SEVERITY_FATAL_ERROR:
                hive::LogError(LOG_DILIGENT, message);
                break;
            default:
                break;
        }
    }

    bool InitSystem() {
        Diligent::SetDebugMessageCallback(&DiligentToHiveMessageCallback);
        return true;
    }

    void ShutdownSystem() {}

    void ShutdownRenderContext(RenderContext& renderContext) {
        if (renderContext.m_swapchain != nullptr)
        {
            renderContext.m_swapchain->Release();
        }

        if (renderContext.m_context != nullptr)
        {
            renderContext.m_context->Release();
        }

        if (renderContext.m_device != nullptr)
        {
            renderContext.m_device->Release();
        }
    }

    void BeginFrame(RenderContext* ctx) {
        using namespace Diligent;
        ITextureView* pRTV = ctx->m_swapchain->GetCurrentBackBufferRTV();
        ITextureView* pDSV = ctx->m_swapchain->GetDepthBufferDSV();
        ctx->m_context->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float clearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
        ctx->m_context->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->m_context->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    void EndFrame(RenderContext* ctx) {
        ctx->m_swapchain->Present();
    }

    void WaitForIdle(RenderContext* ctx) {
        if (ctx == nullptr || ctx->m_context == nullptr)
        {
            return;
        }

        ctx->m_context->Flush();
        ctx->m_context->WaitForIdle();

        if (ctx->m_device != nullptr)
        {
            ctx->m_device->IdleGPU();
        }
    }

    void ResizeSwapchain(RenderContext* ctx, uint32_t width, uint32_t height) {
        if (ctx->m_swapchain != nullptr && width > 0 && height > 0)
            ctx->m_swapchain->Resize(width, height);
    }

    void DrawPipeline(RenderContext* ctx) {
        using namespace Diligent;
        ctx->m_context->SetPipelineState(ctx->m_pipeline);
        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 3;
        ctx->m_context->Draw(drawAttrs);
    }

    void Render(RenderContext* renderContext) {
        using namespace Diligent;

        // Clear the back buffer
        const float clearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
        // Let the engine perform required state transitions
        ITextureView* pRTV = renderContext->m_swapchain->GetCurrentBackBufferRTV();
        ITextureView* pDSV = renderContext->m_swapchain->GetDepthBufferDSV();
        renderContext->m_context->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        renderContext->m_context->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        renderContext->m_context->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0,
                                                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Set the pipeline state in the immediate context
        renderContext->m_context->SetPipelineState(renderContext->m_pipeline);

        // Typically we should now call CommitShaderResources(), however shaders in this example don't
        // use any resources.

        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 3; // We will render 3 vertices
        renderContext->m_context->Draw(drawAttrs);

        renderContext->m_swapchain->Present();
    }

    static const char* g_vsSource = R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
};

void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn)
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);

    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue

    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

    // Pixel shader simply outputs interpolated vertex color
    static const char* g_psSource = R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";

    // ---- Viewport render target (offscreen) ----

    struct ViewportRT
    {
        Diligent::RefCntAutoPtr<Diligent::ITexture> m_color;
        Diligent::RefCntAutoPtr<Diligent::ITexture> m_depth;
        Diligent::ITextureView* m_rtv{nullptr};
        Diligent::ITextureView* m_dsv{nullptr};
        Diligent::ITextureView* m_srv{nullptr};
        Diligent::IRenderDevice* m_device{nullptr};
        uint32_t m_width{0};
        uint32_t m_height{0};
    };

    static void CreateViewportRTTextures(ViewportRT* rt, uint32_t w, uint32_t h, Diligent::TEXTURE_FORMAT fmt) {
        using namespace Diligent;
        rt->m_color.Release();
        rt->m_depth.Release();
        rt->m_width = w;
        rt->m_height = h;

        TextureDesc colorDesc;
        colorDesc.Name = "ViewportRT Color";
        colorDesc.Type = RESOURCE_DIM_TEX_2D;
        colorDesc.Width = w;
        colorDesc.Height = h;
        colorDesc.Format = fmt;
        colorDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        rt->m_device->CreateTexture(colorDesc, nullptr, &rt->m_color);
        rt->m_rtv = rt->m_color->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        rt->m_srv = rt->m_color->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        TextureDesc depthDesc;
        depthDesc.Name = "ViewportRT Depth";
        depthDesc.Type = RESOURCE_DIM_TEX_2D;
        depthDesc.Width = w;
        depthDesc.Height = h;
        depthDesc.Format = TEX_FORMAT_D32_FLOAT;
        depthDesc.BindFlags = BIND_DEPTH_STENCIL;
        rt->m_device->CreateTexture(depthDesc, nullptr, &rt->m_depth);
        rt->m_dsv = rt->m_depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    }

    ViewportRT* CreateViewportRT(RenderContext* ctx, uint32_t width, uint32_t height) {
        auto* rt = new ViewportRT();
        rt->m_device = ctx->m_device;
        auto fmt = ctx->m_swapchain->GetDesc().ColorBufferFormat;
        CreateViewportRTTextures(rt, width, height, fmt);
        return rt;
    }

    void DestroyViewportRT(ViewportRT* rt) {
        delete rt;
    }

    void ResizeViewportRT(ViewportRT* rt, uint32_t width, uint32_t height) {
        if (width > 0 && height > 0 && (rt->m_width != width || rt->m_height != height))
        {
            auto fmt = rt->m_color->GetDesc().Format;
            CreateViewportRTTextures(rt, width, height, fmt);
        }
    }

    uint32_t GetViewportRTWidth(const ViewportRT* rt) {
        return rt->m_width;
    }
    uint32_t GetViewportRTHeight(const ViewportRT* rt) {
        return rt->m_height;
    }
    void* GetViewportRTSRV(const ViewportRT* rt) {
        return rt->m_srv;
    }

    void BeginViewportRT(RenderContext* ctx, ViewportRT* rt) {
        using namespace Diligent;
        ctx->m_context->SetRenderTargets(1, &rt->m_rtv, rt->m_dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const float clearColor[] = {0.180f, 0.180f, 0.180f, 1.0f};
        ctx->m_context->ClearRenderTarget(rt->m_rtv, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->m_context->ClearDepthStencil(rt->m_dsv, CLEAR_DEPTH_FLAG, 1.f, 0,
                                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    void EndViewportRT(RenderContext* ctx, ViewportRT* rt) {
        using namespace Diligent;

        // The offscreen color target is sampled by ImGui in the same frame.
        // Transition it explicitly before leaving the viewport pass.
        const StateTransitionDesc barrier{rt->m_color, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE,
                                          STATE_TRANSITION_FLAG_UPDATE_STATE};
        ctx->m_context->TransitionResourceStates(1, &barrier);
        ctx->m_context->SetRenderTargets(0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

    void SetupGraphicPipeline(RenderContext& renderContext) {
        using namespace Diligent;
        // Pipeline state object encompasses configuration of all GPU stages

        GraphicsPipelineStateCreateInfo psoCreateInfo;

        // Pipeline state name is used by the engine to report issues.
        // It is always a good idea to give objects descriptive names.
        psoCreateInfo.PSODesc.Name = "Simple triangle PSO";

        // This is a graphics pipeline
        psoCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        // clang-format off
    // This tutorial will render to a single render target
    psoCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    psoCreateInfo.GraphicsPipeline.RTVFormats[0]                = renderContext.m_swapchain->GetDesc().ColorBufferFormat;
    // Use the depth buffer format from the swap chain
    psoCreateInfo.GraphicsPipeline.DSVFormat                    = renderContext.m_swapchain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    psoCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // No back face culling for this tutorial
    psoCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Disable depth testing
    psoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        // clang-format on

        ShaderCreateInfo shaderCi;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL under the hood.
        shaderCi.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        shaderCi.Desc.UseCombinedTextureSamplers = true;
        // Create a vertex shader
        RefCntAutoPtr<IShader> pVS;
        {
            shaderCi.Desc.ShaderType = SHADER_TYPE_VERTEX;
            shaderCi.EntryPoint = "main";
            shaderCi.Desc.Name = "Triangle vertex shader";
            shaderCi.Source = g_vsSource;
            renderContext.m_device->CreateShader(shaderCi, &pVS);
        }

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            shaderCi.Desc.ShaderType = SHADER_TYPE_PIXEL;
            shaderCi.EntryPoint = "main";
            shaderCi.Desc.Name = "Triangle pixel shader";
            shaderCi.Source = g_psSource;
            renderContext.m_device->CreateShader(shaderCi, &pPS);
        }

        // Finally, create the pipeline state
        psoCreateInfo.pVS = pVS;
        psoCreateInfo.pPS = pPS;
        renderContext.m_device->CreateGraphicsPipelineState(psoCreateInfo, &renderContext.m_pipeline);
    }
} // namespace swarm
