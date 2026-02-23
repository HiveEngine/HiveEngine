#include <swarm/precomp.h>
#include <swarm/swarm.h>
#include <swarm/swarm_log.h>
#include <swarm/platform/diligent_swarm.h>

#include <hive/core/log.h>

#include <EngineFactoryVk.h>
#include <Shader.h>
#include <RefCntAutoPtr.hpp>
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

    void ShutdownRenderContext(RenderContext &renderContext)
    {
        if (renderContext.swapchain_)
        {
            renderContext.swapchain_->Release();
        }

        if (renderContext.context_)
        {
            renderContext.context_->Release();
        }

        if (renderContext.device_)
        {
            renderContext.device_->Release();
        }
    }


    void BeginFrame(RenderContext* ctx)
    {
        using namespace Diligent;
        ITextureView* pRTV = ctx->swapchain_->GetCurrentBackBufferRTV();
        ITextureView* pDSV = ctx->swapchain_->GetDepthBufferDSV();
        ctx->context_->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
        ctx->context_->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->context_->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    void EndFrame(RenderContext* ctx)
    {
        ctx->swapchain_->Present();
    }

    void ResizeSwapchain(RenderContext* ctx, uint32_t width, uint32_t height)
    {
        if (ctx->swapchain_ && width > 0 && height > 0)
            ctx->swapchain_->Resize(width, height);
    }

    void DrawPipeline(RenderContext *ctx)
    {
        using namespace Diligent;
        ctx->context_->SetPipelineState(ctx->pipeline_);
        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 3;
        ctx->context_->Draw(drawAttrs);
    }

    void Render(RenderContext *renderContext)
    {
        using namespace Diligent;

        // Clear the back buffer
        const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
        // Let the engine perform required state transitions
        ITextureView* pRTV = renderContext->swapchain_->GetCurrentBackBufferRTV();
        ITextureView* pDSV = renderContext->swapchain_->GetDepthBufferDSV();
        renderContext->context_->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        renderContext->context_->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        renderContext->context_->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Set the pipeline state in the immediate context
        renderContext->context_->SetPipelineState(renderContext->pipeline_);

        // Typically we should now call CommitShaderResources(), however shaders in this example don't
        // use any resources.

        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 3; // We will render 3 vertices
        renderContext->context_->Draw(drawAttrs);

        renderContext->swapchain_->Present();
    }



    static const char* VSSource = R"(
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
    static const char* PSSource = R"(
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
        Diligent::RefCntAutoPtr<Diligent::ITexture> color;
        Diligent::RefCntAutoPtr<Diligent::ITexture> depth;
        Diligent::ITextureView* rtv{nullptr};
        Diligent::ITextureView* dsv{nullptr};
        Diligent::ITextureView* srv{nullptr};
        Diligent::IRenderDevice* device{nullptr};
        uint32_t width{0};
        uint32_t height{0};
    };

    static void CreateViewportRTTextures(ViewportRT* rt, uint32_t w, uint32_t h, Diligent::TEXTURE_FORMAT fmt)
    {
        using namespace Diligent;
        rt->color.Release();
        rt->depth.Release();
        rt->width = w;
        rt->height = h;

        TextureDesc colorDesc;
        colorDesc.Name      = "ViewportRT Color";
        colorDesc.Type      = RESOURCE_DIM_TEX_2D;
        colorDesc.Width     = w;
        colorDesc.Height    = h;
        colorDesc.Format    = fmt;
        colorDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        rt->device->CreateTexture(colorDesc, nullptr, &rt->color);
        rt->rtv = rt->color->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        rt->srv = rt->color->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        TextureDesc depthDesc;
        depthDesc.Name      = "ViewportRT Depth";
        depthDesc.Type      = RESOURCE_DIM_TEX_2D;
        depthDesc.Width     = w;
        depthDesc.Height    = h;
        depthDesc.Format    = TEX_FORMAT_D32_FLOAT;
        depthDesc.BindFlags = BIND_DEPTH_STENCIL;
        rt->device->CreateTexture(depthDesc, nullptr, &rt->depth);
        rt->dsv = rt->depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    }

    ViewportRT* CreateViewportRT(RenderContext* ctx, uint32_t width, uint32_t height)
    {
        auto* rt = new ViewportRT();
        rt->device = ctx->device_;
        auto fmt = ctx->swapchain_->GetDesc().ColorBufferFormat;
        CreateViewportRTTextures(rt, width, height, fmt);
        return rt;
    }

    void DestroyViewportRT(ViewportRT* rt)
    {
        delete rt;
    }

    void ResizeViewportRT(ViewportRT* rt, uint32_t width, uint32_t height)
    {
        if (width > 0 && height > 0 && (rt->width != width || rt->height != height))
        {
            auto fmt = rt->color->GetDesc().Format;
            CreateViewportRTTextures(rt, width, height, fmt);
        }
    }

    uint32_t GetViewportRTWidth(const ViewportRT* rt) { return rt->width; }
    uint32_t GetViewportRTHeight(const ViewportRT* rt) { return rt->height; }
    void* GetViewportRTSRV(const ViewportRT* rt) { return rt->srv; }

    void BeginViewportRT(RenderContext* ctx, ViewportRT* rt)
    {
        using namespace Diligent;
        ctx->context_->SetRenderTargets(1, &rt->rtv, rt->dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const float ClearColor[] = {0.180f, 0.180f, 0.180f, 1.0f};
        ctx->context_->ClearRenderTarget(rt->rtv, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->context_->ClearDepthStencil(rt->dsv, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    void EndViewportRT(RenderContext* ctx, ViewportRT* rt)
    {
        (void)rt;
        // Transition back to shader resource happens automatically via Diligent state tracking.
        // Just reset render targets so Diligent knows we're done.
        ctx->context_->SetRenderTargets(0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

    void SetupGraphicPipeline(RenderContext &renderContext)
    {
        using namespace Diligent;
        // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Simple triangle PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = renderContext.swapchain_->GetDesc().ColorBufferFormat;
    // Use the depth buffer format from the swap chain
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = renderContext.swapchain_->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // No back face culling for this tutorial
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Disable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Triangle vertex shader";
        ShaderCI.Source          = VSSource;
        renderContext.device_->CreateShader(ShaderCI, &pVS);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Triangle pixel shader";
        ShaderCI.Source          = PSSource;
        renderContext.device_->CreateShader(ShaderCI, &pPS);
    }

    // Finally, create the pipeline state
    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    renderContext.device_->CreateGraphicsPipelineState(PSOCreateInfo, &renderContext.pipeline_);
    }
}
